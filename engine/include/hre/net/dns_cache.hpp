#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <winsock2.h>

namespace hre::net {

struct dns_entry {
    std::vector<std::string> addresses;
    std::chrono::steady_clock::time_point resolved_at;
    uint32_t ttl_seconds;
    bool negative;

    dns_entry() : ttl_seconds(300), negative(false) {}
    bool expired() const;
};

class dns_cache {
public:
    static dns_cache& instance();

    std::vector<std::string> resolve(const std::string& host);
    std::vector<std::string> lookup(const std::string& host);
    void store(const std::string& host, const std::vector<std::string>& addresses, uint32_t ttl = 300);
    void store_negative(const std::string& host, uint32_t ttl = 60);
    void clear();
    void set_capacity(size_t entries);
    size_t size() const { return m_entries.size(); }

private:
    dns_cache();
    ~dns_cache();
    dns_cache(const dns_cache&) = delete;
    dns_cache& operator=(const dns_cache&) = delete;

    std::map<std::string, dns_entry> m_entries;
    std::vector<std::string> m_access_order;
    size_t m_max_entries;
    mutable std::mutex m_mutex;

    void touch(const std::string& host);
    void evict_if_needed();
    std::vector<std::string> resolve_system(const std::string& host);
};

} // namespace hre::net
