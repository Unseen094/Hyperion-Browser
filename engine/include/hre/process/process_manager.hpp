#pragma once

#include <hre/process/process_manager.hpp>
#include <hre/ipc/ipc_channel.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

namespace hre::process {

// ---- Process Isolation Domain (Phase 32) ---------------------------------

struct origin_triple {
    std::wstring scheme;
    std::wstring host;
    int port = 0;

    bool operator<(const origin_triple& other) const {
        if (scheme != other.scheme) return scheme < other.scheme;
        if (host != other.host) return host < other.host;
        return port < other.port;
    }
    bool operator==(const origin_triple& other) const {
        return scheme == other.scheme && host == other.host && port == other.port;
    }
    std::wstring to_string() const;
    static origin_triple parse(const std::wstring& url);
};

// ---- Process Type / State -------------------------------------------------

enum class process_type {
    RENDERER,
    GPU,
    UTILITY,
    SANDBOX,
    EXTENSION,
    PRIVILEGED
};

enum class process_state {
    STARTING,
    RUNNING,
    STOPPED,
    CRASHED
};

struct process_info {
    uint32_t pid = 0;
    process_type type = process_type::RENDERER;
    process_state state = process_state::STOPPED;
    std::wstring name;
    uint64_t memory_usage = 0;
    double cpu_usage = 0.0;
};

// ---- Process Host Mapping -------------------------------------------------

struct process_host_map {
    uint32_t process_pid = 0;
    process_type type = process_type::RENDERER;
    std::vector<origin_triple> hosted_origins;
    bool is_isolated = false;
    int memory_budget_mb = 512;
};

// Extended process manager with site isolation support
class site_isolation_manager {
public:
    site_isolation_manager();
    ~site_isolation_manager();

    // Site isolation
    uint32_t create_isolated_renderer(const origin_triple& origin);
    bool assign_origin_to_process(const origin_triple& origin, uint32_t pid);
    bool is_origin_isolated(const origin_triple& origin) const;

    uint32_t find_process_for_origin(const origin_triple& origin) const;
    const std::vector<process_host_map>& get_process_map() const { return m_process_map; }

    // Cross-origin request blocking
    bool can_access(origin_triple requesting_origin, origin_triple target_origin, const std::wstring& resource_type) const;
    bool is_same_origin(const origin_triple& a, const origin_triple& b) const;

    // Cross-Origin Read Blocking (CORB)
    bool should_block_response(const origin_triple& requesting_origin,
                                const origin_triple& target_origin,
                                const std::wstring& mime_type) const;

    // Origin-wide storage partitioning
    std::wstring partition_key_for_origin(const origin_triple& origin) const;

    // Process limit
    void set_max_renderer_processes(int max) { m_max_renderers = max; }
    int max_renderer_processes() const { return m_max_renderers; }

private:
    std::vector<process_host_map> m_process_map;
    int m_max_renderers = 6;
    uint32_t m_next_virtual_pid = 1000;
};

// ---- IPC Channel Manager (extends hre::ipc) -------------------------------

class ipc_channel_manager {
public:
    ipc_channel_manager();
    ~ipc_channel_manager();

    bool create_channel_pair(uint32_t parent_pid, uint32_t child_pid);
    bool send_to_process(uint32_t pid, const void* data, size_t size);
    bool broadcast_to_processes(const void* data, size_t size, process_type type);
    void close_channel(uint32_t pid);

    using global_message_handler = std::function<void(uint32_t source_pid, const void*, size_t)>;
    void set_global_handler(global_message_handler handler) { m_global_handler = handler; }

private:
    struct channel_pair {
        ipc::channel parent_side;
        ipc::channel child_side;
    };
    std::map<uint32_t, channel_pair> m_channels;
    global_message_handler m_global_handler;
};

// ---- Process Launcher (extended) ------------------------------------------

class process_launcher {
public:
    struct launch_options {
        std::wstring executable;
        std::wstring args;
        std::wstring working_directory;
        bool inherit_handles = false;
        bool create_window = false;
        bool suspended = false;
        int memory_limit_mb = 0;
        int cpu_limit_percent = 0;
    };

    static uint32_t launch(const launch_options& opts);
    static bool terminate(uint32_t pid, int exit_code);
    static bool is_running(uint32_t pid);
    static int get_exit_code(uint32_t pid);
    static bool set_priority(uint32_t pid, int priority_class);
    static uint64_t get_memory_usage(uint32_t pid);
    static double get_cpu_usage(uint32_t pid);
};

// ---- Process Monitor ------------------------------------------------------

class process_monitor {
public:
    process_monitor();
    ~process_monitor();

    void start_monitoring(uint32_t pid);
    void stop_monitoring(uint32_t pid);
    void monitor_all();

    using crash_callback = std::function<void(uint32_t pid, int exit_code)>;
    void set_crash_callback(crash_callback cb) { m_crash_cb = cb; }

    using memory_warning_callback = std::function<void(uint32_t pid, uint64_t memory_bytes)>;
    void set_memory_warning_callback(memory_warning_callback cb) { m_memory_warning_cb = cb; }
    void set_memory_warning_threshold(uint64_t bytes) { m_memory_threshold = bytes; }

private:
    std::vector<uint32_t> m_monitored_pids;
    crash_callback m_crash_cb;
    memory_warning_callback m_memory_warning_cb;
    uint64_t m_memory_threshold = 1024 * 1024 * 1024; // 1GB
};

// ---- Process Manager ------------------------------------------------------

class process_manager {
public:
    process_manager();
    ~process_manager();

    uint32_t launch_process(process_type type, const std::wstring& executable_path, const std::wstring& args);
    bool terminate_process(uint32_t pid);
    process_info get_process_info(uint32_t pid) const;

    void set_process_exit_callback(uint32_t pid, std::function<void(int)> callback);
    void set_process_error_callback(std::function<void(uint32_t, const std::wstring&)> callback);

    void update_process_list();
    bool is_process_running(uint32_t pid) const;
    void shutdown_all();

    static uint32_t current_process_id();

private:
    std::map<uint32_t, process_info> m_processes;
    std::map<uint32_t, std::function<void(int)>> m_exit_callbacks;
    std::function<void(uint32_t, const std::wstring&)> m_error_callback;
};

// ---- IPC Endpoint ---------------------------------------------------------

class ipc_endpoint {
public:
    ipc_endpoint();
    ~ipc_endpoint();

    bool connect_to_process(uint32_t pid);
    bool listen(const std::wstring& endpoint_name);
    void disconnect();
    bool send(const void* data, size_t size);
    size_t receive(void* buffer, size_t max_size);

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
    bool m_connected = false;
};

} // namespace hre::process
