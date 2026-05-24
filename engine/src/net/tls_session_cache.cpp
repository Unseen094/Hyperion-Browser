#include "hre/net/tls_session_cache.hpp"
#include <algorithm>

namespace hre::net {

bool tls_session_data::expired() const {
    if (lifetime_hint == 0) return false;
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - created_at).count();
    return static_cast<uint64_t>(age) >= lifetime_hint;
}

tls_session_cache::tls_session_cache() {}
tls_session_cache::~tls_session_cache() { clear(); }

tls_session_cache& tls_session_cache::instance() {
    static tls_session_cache cache;
    return cache;
}

void tls_session_cache::store(const std::string& host, const tls_session_data& session) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions[host] = session;
}

std::vector<uint8_t> tls_session_cache::lookup_ticket(const std::string& host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(host);
    if (it == m_sessions.end()) return {};
    if (it->second.expired()) {
        m_sessions.erase(it);
        return {};
    }
    return it->second.session_ticket;
}

std::vector<uint8_t> tls_session_cache::lookup_master_secret(const std::string& host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(host);
    if (it == m_sessions.end()) return {};
    return it->second.master_secret;
}

bool tls_session_cache::has_session(const std::string& host) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(host);
    if (it == m_sessions.end()) return false;
    if (it->second.expired()) return false;
    return !it->second.session_ticket.empty();
}

void tls_session_cache::remove(const std::string& host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.erase(host);
}

void tls_session_cache::clear_expired() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        if (it->second.expired()) {
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

void tls_session_cache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.clear();
}

} // namespace hre::net
