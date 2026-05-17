#include "../include/hyperion/platform/process_manager.hpp"
#include "../include/hyperion/platform/logging.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <codecvt>
#include <locale>

namespace hyperion {
namespace platform {

static std::string to_utf8(const std::wstring& ws) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws);
#pragma warning(pop)
}

process_manager::process_manager() {}

process_manager::~process_manager() {
    for (auto& [pid, renderer] : m_renderers) {
        if (renderer.process_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(renderer.process_handle);
        }
        if (renderer.job_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(renderer.job_handle);
        }
    }
    m_renderers.clear();
}

bool process_manager::spawn_renderer(const std::string& pipe_name) {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) {
        HYPERION_LOG_ERROR("Failed to create Job Object. Error: {}", GetLastError());
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limit = { 0 };
    job_limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION |
                                                 JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                                 JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    job_limit.BasicLimitInformation.ActiveProcessLimit = 1;

    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &job_limit, sizeof(job_limit))) {
        HYPERION_LOG_ERROR("Failed to set Job limits. Error: {}", GetLastError());
        CloseHandle(job);
        return false;
    }

    JOBOBJECT_BASIC_UI_RESTRICTIONS ui_limits = { 0 };
    ui_limits.UIRestrictionsClass = JOB_OBJECT_UILIMIT_HANDLES |
                                    JOB_OBJECT_UILIMIT_READCLIPBOARD |
                                    JOB_OBJECT_UILIMIT_WRITECLIPBOARD |
                                    JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                                    JOB_OBJECT_UILIMIT_DISPLAYSETTINGS |
                                    JOB_OBJECT_UILIMIT_GLOBALATOMS;

    if (!SetInformationJobObject(job, JobObjectBasicUIRestrictions, &ui_limits, sizeof(ui_limits))) {
        HYPERION_LOG_ERROR("Failed to set Job UI limits. Error: {}", GetLastError());
        CloseHandle(job);
        return false;
    }

    wchar_t exe_path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0) {
        HYPERION_LOG_ERROR("Failed to get current module filename. Error: {}", GetLastError());
        CloseHandle(job);
        return false;
    }

    std::wstring exe_str(exe_path);
    size_t last_slash = exe_str.find_last_of(L"\\/");
    std::wstring bin_dir = (last_slash != std::wstring::npos) ? exe_str.substr(0, last_slash + 1) : L"";
    std::wstring renderer_path = bin_dir + L"hyperion_renderer.exe";

    HYPERION_LOG_INFO("Spawning renderer process: {}", to_utf8(renderer_path));

    std::wstring pipe_name_w(pipe_name.begin(), pipe_name.end());
    std::wstring cmd = L"\"" + renderer_path + L"\" --type=renderer --pipe=" + pipe_name_w;

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    std::vector<wchar_t> cmd_buffer(cmd.begin(), cmd.end());
    cmd_buffer.push_back(L'\0');

    if (!CreateProcessW(
            nullptr,
            cmd_buffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB | CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        HYPERION_LOG_ERROR("Failed to create renderer process. Error: {}", GetLastError());
        CloseHandle(job);
        return false;
    }

    if (!AssignProcessToJobObject(job, pi.hProcess)) {
        HYPERION_LOG_ERROR("Failed to assign process to Job. Error: {}", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(job);
        return false;
    }

    ResumeThread(pi.hThread);

    m_renderers[pi.dwProcessId] = { pi.dwProcessId, job, pi.hProcess };

    HYPERION_LOG_INFO("Renderer process spawned with PID: {}", pi.dwProcessId);
    return true;
}

bool process_manager::terminate_renderer(DWORD process_id) {
    auto it = m_renderers.find(process_id);
    if (it == m_renderers.end()) {
        HYPERION_LOG_WARN("Cannot terminate: renderer {} not found", process_id);
        return false;
    }

    if (it->second.process_handle != INVALID_HANDLE_VALUE) {
        TerminateProcess(it->second.process_handle, 0);
        CloseHandle(it->second.process_handle);
    }
    if (it->second.job_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(it->second.job_handle);
    }

    m_renderers.erase(it);
    HYPERION_LOG_INFO("Renderer process {} terminated", process_id);
    return true;
}

std::vector<DWORD> process_manager::get_renderer_ids() {
    std::vector<DWORD> ids;
    for (const auto& [pid, _] : m_renderers) {
        ids.push_back(pid);
    }
    return ids;
}

} // namespace platform
} // namespace hyperion
