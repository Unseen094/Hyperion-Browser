#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <cstdint>

namespace hre::net {

struct tls_session_data {
    std::vector<uint8_t> session_ticket;
    std::vector<uint8_t> master_secret;
    std::vector<uint8_t> server_cert_hash;
    std::chrono::steady_clock::time_point created_at;
    uint32_t lifetime_hint; // seconds

    bool expired() const;
};

class tls_session_cache {
public:
    static tls_session_cache& instance();

    void store(const std::string& host, const tls_session_data& session);
    std::vector<uint8_t> lookup_ticket(const std::string& host);
    std::vector<uint8_t> lookup_master_secret(const std::string& host);
    bool has_session(const std::string& host) const;

    void remove(const std::string& host);
    void clear_expired();
    void clear();
    size_t size() const { return m_sessions.size(); }

private:
    tls_session_cache();
    ~tls_session_cache();
    tls_session_cache(const tls_session_cache&) = delete;
    tls_session_cache& operator=(const tls_session_cache&) = delete;

    std::map<std::string, tls_session_data> m_sessions;
    mutable std::mutex m_mutex;
};

} // namespace hre::net
