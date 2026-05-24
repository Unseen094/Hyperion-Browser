#include "hre/net/resource_hints.hpp"
#include "hre/net/dns_cache.hpp"
#include <ws2tcpip.h>
#include <thread>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace hre::net {

preconnect_handle::preconnect_handle(preconnect_handle&& other) noexcept
    : host(std::move(other.host))
    , port(other.port)
    , use_tls(other.use_tls)
    , sock(other.sock)
    , connected(other.connected)
    , created_at(other.created_at) {
    other.sock = INVALID_SOCKET;
    other.connected = false;
}

preconnect_handle& preconnect_handle::operator=(preconnect_handle&& other) noexcept {
    if (this != &other) {
        close();
        host = std::move(other.host);
        port = other.port;
        use_tls = other.use_tls;
        sock = other.sock;
        connected = other.connected;
        created_at = other.created_at;
        other.sock = INVALID_SOCKET;
        other.connected = false;
    }
    return *this;
}

void preconnect_handle::close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    connected = false;
}

preconnect_handle::~preconnect_handle() { close(); }

resource_hints_manager::resource_hints_manager() {}

resource_hints_manager::~resource_hints_manager() { clear_all(); }

resource_hints_manager& resource_hints_manager::instance() {
    static resource_hints_manager mgr;
    return mgr;
}

void resource_hints_manager::preconnect(const std::string& host, int port, bool use_tls) {
    auto now = std::chrono::steady_clock::now();

    m_preconnects.erase(std::remove_if(m_preconnects.begin(), m_preconnects.end(),
        [now](const preconnect_handle& h) {
            return !h.connected && (now - h.created_at > CONNECT_TIMEOUT);
        }), m_preconnects.end());

    for (const auto& h : m_preconnects) {
        if (h.host == host && h.port == port && h.use_tls == use_tls) return;
    }

    preconnect_handle handle;
    handle.host = host;
    handle.port = port;
    handle.use_tls = use_tls;
    handle.created_at = now;

    handle.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle.sock == INVALID_SOCKET) return;

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* info = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &info) != 0) {
        handle.close();
        return;
    }

    u_long nonblock = 1;
    ioctlsocket(handle.sock, FIONBIO, &nonblock);

    ::connect(handle.sock, info->ai_addr, static_cast<int>(info->ai_addrlen));
    freeaddrinfo(info);

    fd_set write_set;
    timeval tv = {2, 0};
    FD_ZERO(&write_set);
    FD_SET(handle.sock, &write_set);
    if (select(0, nullptr, &write_set, nullptr, &tv) > 0) {
        handle.connected = true;
    }

    if (!handle.connected) {
        handle.close();
        return;
    }

    m_preconnects.push_back(std::move(handle));
}

void resource_hints_manager::prefetch(const std::string& url, std::function<void(bool)> callback) {
    if (m_prefetched.count(url)) {
        if (callback) callback(m_prefetched[url]);
        return;
    }

    std::thread([this, url, callback]() {
        // Simple fetch to warm the cache
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            if (callback) callback(false);
            return;
        }

        std::string host;
        std::string path = "/";
        int port = 80;

        size_t proto_end = url.find("://");
        if (proto_end != std::string::npos) {
            std::string host_part = url.substr(proto_end + 3);
            size_t path_start = host_part.find("/");
            if (path_start != std::string::npos) {
                host = host_part.substr(0, path_start);
                path = host_part.substr(path_start);
            } else {
                host = host_part;
            }
        }

        addrinfo hints = {}, *info = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &info) != 0) {
            if (callback) callback(false);
            WSACleanup();
            return;
        }

        SOCKET s = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (s == INVALID_SOCKET) {
            freeaddrinfo(info);
            if (callback) callback(false);
            WSACleanup();
            return;
        }

        if (::connect(s, info->ai_addr, static_cast<int>(info->ai_addrlen)) == SOCKET_ERROR) {
            closesocket(s);
            freeaddrinfo(info);
            if (callback) callback(false);
            WSACleanup();
            return;
        }
        freeaddrinfo(info);

        std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
        ::send(s, req.c_str(), static_cast<int>(req.size()), 0);

        char buf[4096];
        while (::recv(s, buf, sizeof(buf), 0) > 0) {}

        closesocket(s);
        WSACleanup();

        m_prefetched[url] = true;
        if (callback) callback(true);
        if (m_prefetch_cb) m_prefetch_cb(url, true);
    }).detach();
}

void resource_hints_manager::preload(const std::string& url, const std::string& as) {
    resource_hint hint;
    hint.type = resource_hint_type::PRELOAD;
    hint.url = url;
    hint.as = as;
    add_hint(hint);
    prefetch(url);
}

void resource_hints_manager::preload_dns(const std::string& host) {
    resource_hint hint;
    hint.type = resource_hint_type::PRELOAD_DNS;
    hint.url = host;
    add_hint(hint);
}

void resource_hints_manager::add_hint(const resource_hint& hint) {
    switch (hint.type) {
        case resource_hint_type::PRECONNECT:
            preconnect(hint.url, 443, true);
            break;
        case resource_hint_type::PREFETCH:
            prefetch(hint.url);
            break;
        case resource_hint_type::PRELOAD:
            prefetch(hint.url);
            break;
        case resource_hint_type::PRELOAD_DNS:
            dns_cache::instance().resolve(hint.url);
            break;
        default:
            break;
    }
}

void resource_hints_manager::clear_idle_connects() {
    auto now = std::chrono::steady_clock::now();
    m_preconnects.erase(std::remove_if(m_preconnects.begin(), m_preconnects.end(),
        [now](const preconnect_handle& h) {
            return !h.connected || (now - h.created_at > CONNECT_TIMEOUT);
        }), m_preconnects.end());
}

void resource_hints_manager::clear_all() {
    m_preconnects.clear();
    m_prefetched.clear();
}

} // namespace hre::net
