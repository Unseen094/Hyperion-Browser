#include "hre/net/dns_cache.hpp"
#include <ws2tcpip.h>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

namespace hre::net {

bool dns_entry::expired() const {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - resolved_at).count();
    return static_cast<uint64_t>(age) >= ttl_seconds;
}

dns_cache::dns_cache() : m_max_entries(256) {}

dns_cache::~dns_cache() { clear(); }

dns_cache& dns_cache::instance() {
    static dns_cache cache;
    return cache;
}

void dns_cache::set_capacity(size_t entries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_entries = entries;
    while (m_access_order.size() > m_max_entries) {
        m_entries.erase(m_access_order.back());
        m_access_order.pop_back();
    }
}

void dns_cache::touch(const std::string& host) {
    auto it = std::find(m_access_order.begin(), m_access_order.end(), host);
    if (it != m_access_order.end()) {
        m_access_order.erase(it);
    }
    m_access_order.insert(m_access_order.begin(), host);
}

void dns_cache::evict_if_needed() {
    while (m_access_order.size() > m_max_entries) {
        m_entries.erase(m_access_order.back());
        m_access_order.pop_back();
    }
}

std::vector<std::string> dns_cache::lookup(const std::string& host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(host);
    if (it == m_entries.end()) return {};
    if (it->second.expired()) {
        m_entries.erase(it);
        return {};
    }
    touch(host);
    if (it->second.negative) return {};
    return it->second.addresses;
}

void dns_cache::store(const std::string& host, const std::vector<std::string>& addresses, uint32_t ttl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    dns_entry entry;
    entry.addresses = addresses;
    entry.resolved_at = std::chrono::steady_clock::now();
    entry.ttl_seconds = ttl;
    entry.negative = false;
    m_entries[host] = entry;
    touch(host);
    evict_if_needed();
}

void dns_cache::store_negative(const std::string& host, uint32_t ttl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    dns_entry entry;
    entry.resolved_at = std::chrono::steady_clock::now();
    entry.ttl_seconds = ttl;
    entry.negative = true;
    m_entries[host] = entry;
    touch(host);
    evict_if_needed();
}

void dns_cache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
    m_access_order.clear();
}

std::vector<std::string> dns_cache::resolve(const std::string& host) {
    {
        auto cached = lookup(host);
        if (!cached.empty()) return cached;
    }

    auto addresses = resolve_system(host);
    if (!addresses.empty()) {
        store(host, addresses);
    } else {
        store_negative(host);
    }
    return addresses;
}

std::vector<std::string> dns_cache::resolve_system(const std::string& host) {
    std::vector<std::string> addresses;
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* info = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &info) != 0) {
        return addresses;
    }

    for (addrinfo* p = info; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
            addresses.emplace_back(ip);
        }
    }

    freeaddrinfo(info);
    return addresses;
}

} // namespace hre::net
