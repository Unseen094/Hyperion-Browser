#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

namespace hre::sandbox {

enum class sandbox_type {
    NONE,
    NORMAL,
    RESTRICTED,
    FULL
};

struct sandbox_config {
    bool enable_network = true;
    bool enable_filesystem = false;
    bool enable_processes = false;
    bool enable_shared_memory = true;
    std::vector<std::wstring> allowed_paths;
    std::vector<std::wstring> blocked_paths;
    uint64_t memory_limit = 0;
    uint64_t cpu_time_limit = 0;
};

class sandbox {
public:
    sandbox();
    ~sandbox();

    bool initialize(const sandbox_config& config);
    void terminate();

    bool is_active() const { return m_active; }
    sandbox_type type() const { return m_type; }

    bool execute(void (*entry)(void*), void* user_data);
    bool execute_in_process(const std::wstring& path, const std::wstring& args, uint32_t* out_pid);

    using error_callback = std::function<void(const std::wstring&)>;
    void set_error_callback(error_callback cb) { m_error_callback = cb; }

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
    sandbox_config m_config;
    sandbox_type m_type = sandbox_type::NONE;
    bool m_active = false;
    error_callback m_error_callback;
};

class security_token {
public:
    security_token();
    ~security_token();

    static security_token create_restricted();
    static security_token create_untrusted();

    uint64_t capabilities() const { return m_capabilities; }
    bool has_capability(uint64_t cap) const;

    void add_capability(uint64_t cap);
    void remove_capability(uint64_t cap);

private:
    uint64_t m_capabilities = 0;
};

class capability {
public:
    static constexpr uint64_t NETWORK = 1 << 0;
    static constexpr uint64_t FILESYSTEM_READ = 1 << 1;
    static constexpr uint64_t FILESYSTEM_WRITE = 1 << 2;
    static constexpr uint64_t PROCESS_CREATE = 1 << 3;
    static constexpr uint64_t SHARED_MEMORY = 1 << 4;
    static constexpr uint64_t SOCKET = 1 << 5;
    static constexpr uint64_t THREAD_CREATE = 1 << 6;
};

} // namespace hre::sandbox