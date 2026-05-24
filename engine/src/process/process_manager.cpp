#include <hre/process/process_manager.hpp>
#include <algorithm>
#include <sstream>
#include <windows.h>
#include <psapi.h>

namespace hre::process {

// ---- origin_triple --------------------------------------------------------

std::wstring origin_triple::to_string() const {
    return scheme + L"://" + host + (port > 0 ? L":" + std::to_wstring(port) : L"");
}

origin_triple origin_triple::parse(const std::wstring& url) {
    origin_triple result;
    size_t scheme_end = url.find(L"://");
    if (scheme_end == std::wstring::npos) return result;
    result.scheme = url.substr(0, scheme_end);
    size_t host_start = scheme_end + 3;
    size_t host_end = url.find_first_of(L":/?#", host_start);
    if (host_end == std::wstring::npos) {
        result.host = url.substr(host_start);
        return result;
    }
    result.host = url.substr(host_start, host_end - host_start);
    if (url[host_end] == L':') {
        size_t port_end = url.find_first_of(L"/?#", host_end + 1);
        std::wstring port_str = url.substr(host_end + 1, port_end - host_end - 1);
        result.port = std::stoi(port_str);
    }
    return result;
}

// ---- site_isolation_manager -----------------------------------------------

site_isolation_manager::site_isolation_manager() = default;
site_isolation_manager::~site_isolation_manager() = default;

uint32_t site_isolation_manager::create_isolated_renderer(const origin_triple& origin) {
    process_host_map phm;
    phm.process_pid = m_next_virtual_pid++;
    phm.type = process_type::RENDERER;
    phm.hosted_origins.push_back(origin);
    phm.is_isolated = true;
    m_process_map.push_back(phm);
    return phm.process_pid;
}

bool site_isolation_manager::assign_origin_to_process(const origin_triple& origin, uint32_t pid) {
    for (auto& phm : m_process_map) {
        if (phm.process_pid == pid) {
            phm.hosted_origins.push_back(origin);
            return true;
        }
    }
    return false;
}

bool site_isolation_manager::is_origin_isolated(const origin_triple& origin) const {
    for (const auto& phm : m_process_map) {
        for (const auto& o : phm.hosted_origins) {
            if (o == origin) return phm.is_isolated;
        }
    }
    return false;
}

uint32_t site_isolation_manager::find_process_for_origin(const origin_triple& origin) const {
    for (const auto& phm : m_process_map) {
        for (const auto& o : phm.hosted_origins) {
            if (o == origin) return phm.process_pid;
        }
    }
    return 0;
}

bool site_isolation_manager::can_access(origin_triple requesting_origin, origin_triple target_origin, const std::wstring& resource_type) const {
    if (is_same_origin(requesting_origin, target_origin)) return true;
    if (resource_type == L"document" && is_origin_isolated(target_origin)) return false;
    if (resource_type == L"font" || resource_type == L"media") return true;
    return true;
}

bool site_isolation_manager::is_same_origin(const origin_triple& a, const origin_triple& b) const {
    return a.scheme == b.scheme && a.host == b.host && a.port == b.port;
}

bool site_isolation_manager::should_block_response(const origin_triple& requesting_origin,
                                                     const origin_triple& target_origin,
                                                     const std::wstring& mime_type) const {
    if (is_same_origin(requesting_origin, target_origin)) return false;
    if (mime_type.find(L"text/html") != std::wstring::npos) return false;
    if (mime_type.find(L"application/json") != std::wstring::npos) return true;
    if (mime_type.find(L"text/xml") != std::wstring::npos) return true;
    if (mime_type.find(L"application/pdf") != std::wstring::npos) return true;
    if (mime_type.find(L"text/plain") != std::wstring::npos) return false;
    return false;
}

std::wstring site_isolation_manager::partition_key_for_origin(const origin_triple& origin) const {
    return origin.scheme + L"_" + origin.host + L"_" + std::to_wstring(origin.port);
}

// ---- ipc_channel_manager --------------------------------------------------

ipc_channel_manager::ipc_channel_manager() = default;
ipc_channel_manager::~ipc_channel_manager() = default;

bool ipc_channel_manager::create_channel_pair(uint32_t parent_pid, uint32_t child_pid) {
    channel_pair pair;
    m_channels[parent_pid] = pair;
    m_channels[child_pid] = pair;
    return true;
}

bool ipc_channel_manager::send_to_process(uint32_t pid, const void* data, size_t size) {
    auto it = m_channels.find(pid);
    if (it == m_channels.end()) return false;
    it->second.parent_side.send(data, size);
    return true;
}

bool ipc_channel_manager::broadcast_to_processes(const void* data, size_t size, process_type type) {
    for (const auto& [pid, pair] : m_channels) {
        (void)pair;
        if (pid > 0) {
            send_to_process(pid, data, size);
        }
    }
    return true;
}

void ipc_channel_manager::close_channel(uint32_t pid) {
    m_channels.erase(pid);
}

// ---- process_launcher -----------------------------------------------------

uint32_t process_launcher::launch(const launch_options& opts) {
    std::wstring cmd = opts.executable + L" " + opts.args;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD flags = opts.suspended ? CREATE_SUSPENDED : 0;
    if (!opts.create_window) flags |= CREATE_NO_WINDOW;

    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, opts.inherit_handles,
                        flags, nullptr,
                        opts.working_directory.empty() ? nullptr : opts.working_directory.c_str(),
                        &si, &pi)) {
        CloseHandle(pi.hThread);
        return pi.dwProcessId;
    }
    return 0;
}

bool process_launcher::terminate(uint32_t pid, int exit_code) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    bool result = TerminateProcess(h, static_cast<UINT>(exit_code)) != 0;
    CloseHandle(h);
    return result;
}

bool process_launcher::is_running(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code;
    bool running = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return running;
}

int process_launcher::get_exit_code(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return -1;
    DWORD code;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return static_cast<int>(code);
}

bool process_launcher::set_priority(uint32_t pid, int priority_class) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) return false;
    bool result = SetPriorityClass(h, static_cast<DWORD>(priority_class)) != 0;
    CloseHandle(h);
    return result;
}

uint64_t process_launcher::get_memory_usage(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return 0;
    PROCESS_MEMORY_COUNTERS pmc;
    bool result = GetProcessMemoryInfo(h, &pmc, sizeof(pmc));
    CloseHandle(h);
    return result ? pmc.WorkingSetSize : 0;
}

double process_launcher::get_cpu_usage(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return 0;
    FILETIME create, exit, kernel, user;
    if (GetProcessTimes(h, &create, &exit, &kernel, &user)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
        CloseHandle(h);
        return (k.QuadPart + u.QuadPart) / 10000.0;
    }
    CloseHandle(h);
    return 0;
}

// ---- process_monitor ------------------------------------------------------

process_monitor::process_monitor() = default;
process_monitor::~process_monitor() { m_monitored_pids.clear(); }

void process_monitor::start_monitoring(uint32_t pid) {
    if (std::find(m_monitored_pids.begin(), m_monitored_pids.end(), pid) == m_monitored_pids.end()) {
        m_monitored_pids.push_back(pid);
    }
}

void process_monitor::stop_monitoring(uint32_t pid) {
    auto it = std::remove(m_monitored_pids.begin(), m_monitored_pids.end(), pid);
    m_monitored_pids.erase(it, m_monitored_pids.end());
}

void process_monitor::monitor_all() {
    for (auto it = m_monitored_pids.begin(); it != m_monitored_pids.end(); ) {
        if (!process_launcher::is_running(*it)) {
            int code = process_launcher::get_exit_code(*it);
            if (m_crash_cb) m_crash_cb(*it, code);
            it = m_monitored_pids.erase(it);
        } else {
            if (m_memory_warning_cb) {
                uint64_t mem = process_launcher::get_memory_usage(*it);
                if (mem > m_memory_threshold) m_memory_warning_cb(*it, mem);
            }
            ++it;
        }
    }
}

// ---- original process_manager methods -------------------------------------

process_manager::process_manager() = default;
process_manager::~process_manager() { shutdown_all(); }

uint32_t process_manager::launch_process(process_type type, const std::wstring& executable_path, const std::wstring& args) {
    process_launcher::launch_options opts;
    opts.executable = executable_path;
    opts.args = args;
    uint32_t pid = process_launcher::launch(opts);
    if (pid) {
        process_info info;
        info.pid = pid;
        info.type = type;
        info.state = process_state::STARTING;
        info.name = executable_path;
        m_processes[pid] = info;
    }
    return pid;
}

bool process_manager::terminate_process(uint32_t pid) {
    return process_launcher::terminate(pid, 0);
}

process_info process_manager::get_process_info(uint32_t pid) const {
    auto it = m_processes.find(pid);
    if (it != m_processes.end()) return it->second;
    return {};
}

void process_manager::set_process_exit_callback(uint32_t pid, std::function<void(int)> callback) {
    m_exit_callbacks[pid] = callback;
}

void process_manager::set_process_error_callback(std::function<void(uint32_t, const std::wstring&)> callback) {
    m_error_callback = callback;
}

void process_manager::update_process_list() {
    for (auto& [pid, info] : m_processes) {
        if (process_launcher::is_running(pid)) {
            info.state = process_state::RUNNING;
            info.memory_usage = process_launcher::get_memory_usage(pid);
            info.cpu_usage = process_launcher::get_cpu_usage(pid);
        } else if (info.state == process_state::RUNNING) {
            info.state = process_state::STOPPED;
            auto it = m_exit_callbacks.find(pid);
            if (it != m_exit_callbacks.end()) it->second(0);
        }
    }
}

bool process_manager::is_process_running(uint32_t pid) const {
    auto it = m_processes.find(pid);
    return it != m_processes.end() && it->second.state == process_state::RUNNING;
}

void process_manager::shutdown_all() {
    for (const auto& [pid, _] : m_processes) {
        (void)_;
        process_launcher::terminate(pid, 0);
    }
    m_processes.clear();
}

uint32_t process_manager::current_process_id() {
    return GetCurrentProcessId();
}

// ---- ipc_endpoint methods -------------------------------------------------

struct ipc_endpoint::impl {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    std::wstring name;
};

ipc_endpoint::ipc_endpoint() : m_impl(std::make_unique<impl>()) {}
ipc_endpoint::~ipc_endpoint() { disconnect(); }

bool ipc_endpoint::connect_to_process(uint32_t pid) {
    (void)pid;
    std::wstring pipe_name = L"\\\\.\\pipe\\hyperion_" + std::to_wstring(pid);
    m_impl->pipe = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (m_impl->pipe == INVALID_HANDLE_VALUE) return false;
    m_connected = true;
    return true;
}

bool ipc_endpoint::listen(const std::wstring& endpoint_name) {
    m_impl->name = L"\\\\.\\pipe\\" + endpoint_name;
    m_impl->pipe = CreateNamedPipeW(m_impl->name.c_str(),
                                     PIPE_ACCESS_DUPLEX,
                                     PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                     PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, nullptr);
    if (m_impl->pipe == INVALID_HANDLE_VALUE) return false;
    m_connected = true;
    return true;
}

void ipc_endpoint::disconnect() {
    if (m_impl->pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_impl->pipe);
        m_impl->pipe = INVALID_HANDLE_VALUE;
    }
    m_connected = false;
}

bool ipc_endpoint::send(const void* data, size_t size) {
    if (!m_connected || m_impl->pipe == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    return WriteFile(m_impl->pipe, data, static_cast<DWORD>(size), &written, nullptr) != 0;
}

size_t ipc_endpoint::receive(void* buffer, size_t max_size) {
    if (!m_connected || m_impl->pipe == INVALID_HANDLE_VALUE) return 0;
    DWORD read;
    if (!ReadFile(m_impl->pipe, buffer, static_cast<DWORD>(max_size), &read, nullptr)) return 0;
    return read;
}

} // namespace hre::process
