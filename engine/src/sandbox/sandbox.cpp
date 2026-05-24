#include <hre/sandbox/sandbox.hpp>

namespace hre::sandbox {

struct sandbox::impl {};

sandbox::sandbox() = default;
sandbox::~sandbox() {
    terminate();
}

bool sandbox::initialize(const sandbox_config& config) {
    m_config = config;
    m_active = true;
    return true;
}

void sandbox::terminate() {
    m_active = false;
}

bool sandbox::execute(void (*entry)(void*), void* user_data) {
    if (!m_active) return false;
    if (entry) {
        entry(user_data);
    }
    return true;
}

bool sandbox::execute_in_process(const std::wstring& path, const std::wstring& args, uint32_t* out_pid) {
    return false;
}

security_token::security_token() = default;
security_token::~security_token() = default;

security_token security_token::create_restricted() {
    security_token token;
    token.m_capabilities = capability::NETWORK | capability::SHARED_MEMORY | capability::THREAD_CREATE;
    return token;
}

security_token security_token::create_untrusted() {
    security_token token;
    token.m_capabilities = capability::SHARED_MEMORY;
    return token;
}

bool security_token::has_capability(uint64_t cap) const {
    return (m_capabilities & cap) != 0;
}

void security_token::add_capability(uint64_t cap) {
    m_capabilities |= cap;
}

void security_token::remove_capability(uint64_t cap) {
    m_capabilities &= ~cap;
}

} // namespace hre::sandbox