#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <cstdint>
#include <winsock2.h>

namespace hre::net {

struct preconnect_handle {
    std::string host;
    int port;
    bool use_tls;
    SOCKET sock;
    bool connected;
    std::chrono::steady_clock::time_point created_at;

    preconnect_handle() : port(0), use_tls(false), sock(INVALID_SOCKET), connected(false) {}
    ~preconnect_handle();
    preconnect_handle(const preconnect_handle&) = delete;
    preconnect_handle& operator=(const preconnect_handle&) = delete;
    preconnect_handle(preconnect_handle&& other) noexcept;
    preconnect_handle& operator=(preconnect_handle&& other) noexcept;
    void close();
};

enum class resource_hint_type {
    PREFETCH,
    PRERENDER,
    PRELOAD,
    PRELOAD_DNS,
    PRECONNECT
};

struct resource_hint {
    resource_hint_type type;
    std::string url;
    std::string as;
    std::string crossorigin;
    bool important;
};

class resource_hints_manager {
public:
    static resource_hints_manager& instance();

    void preconnect(const std::string& host, int port, bool use_tls = false);
    void prefetch(const std::string& url, std::function<void(bool)> callback = nullptr);
    void preload(const std::string& url, const std::string& as);
    void preload_dns(const std::string& host);
    void add_hint(const resource_hint& hint);

    void clear_idle_connects();
    void clear_all();
    size_t active_connections() const { return m_preconnects.size(); }

    void on_prefetch_complete(std::function<void(const std::string&, bool)> cb) { m_prefetch_cb = cb; }

private:
    resource_hints_manager();
    ~resource_hints_manager();
    resource_hints_manager(const resource_hints_manager&) = delete;
    resource_hints_manager& operator=(const resource_hints_manager&) = delete;

    std::vector<preconnect_handle> m_preconnects;
    std::map<std::string, bool> m_prefetched;
    std::function<void(const std::string&, bool)> m_prefetch_cb;

    static constexpr auto CONNECT_TIMEOUT = std::chrono::seconds(30);
};

} // namespace hre::net
