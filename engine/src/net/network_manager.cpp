#include <hre/net/network_manager.hpp>
#include <hyperion/platform/logging.hpp>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <vector>
#include <thread>
#include <codecvt>
#include <locale>
//#include <zlib.h>  // not available in current build environment
#include "hre/net/ssl_socket.hpp"
#include "hre/net/cookie_manager.hpp"

#pragma comment(lib, "ws2_32.lib")

namespace hre::net {

static std::string wstring_to_utf8(const std::wstring& ws) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws);
#pragma warning(pop)
}

static std::wstring utf8_to_wstring(const std::string& s) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(s);
#pragma warning(pop)
}

static std::string trim_str(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

static std::vector<std::string> split_str(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(trim_str(item));
    }
    return result;
}

network_manager::network_manager() {
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

network_manager::~network_manager() {
    // Close pooled connections
    for (auto& entry : m_connection_pool) {
        if (entry.socket) {
            closesocket((SOCKET)entry.socket);
            entry.socket = nullptr;
        }
    }
    WSACleanup();
}

bool network_manager::parse_url(const std::string& url, std::string& host, int& port,
                                 std::string& path, bool& ssl) {
    host.clear();
    path = "/";
    port = 80;
    ssl = false;

    std::string url_str = url;
    size_t protocol_end = url_str.find("://");
    if (protocol_end != std::string::npos) {
        std::string protocol = url_str.substr(0, protocol_end);
        ssl = (protocol == "https");
        port = ssl ? 443 : 80;
        url_str = url_str.substr(protocol_end + 3);
    }

    size_t path_start = url_str.find("/");
    if (path_start != std::string::npos) {
        host = url_str.substr(0, path_start);
        path = url_str.substr(path_start);
    } else {
        host = url_str;
    }

    // Extract port from host if present
    size_t colon = host.find(":");
    if (colon != std::string::npos) {
        port = std::stoi(host.substr(colon + 1));
        host = host.substr(0, colon);
    }

    return !host.empty();
}

std::string network_manager::build_request(const std::string& method, const std::string& host,
                                            const std::string& path,
                                            const std::map<std::string, std::string>& headers,
                                            const std::string& body) {
    std::stringstream req;
    req << method << " " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";

    for (const auto& [key, val] : headers) {
        req << key << ": " << val << "\r\n";
    }

    // Default headers
    if (headers.find("User-Agent") == headers.end())
        req << "User-Agent: HyperionBrowser/0.1.0\r\n";
    if (headers.find("Accept") == headers.end())
        req << "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    if (headers.find("Accept-Encoding") == headers.end())
        req << "Accept-Encoding: gzip, deflate\r\n";
    if (headers.find("Connection") == headers.end())
        req << "Connection: keep-alive\r\n";

    // Cookies
    std::string cookie_hdr = cookie_manager::instance().get_cookie_header(host, host.find("https") == 0);
    if (!cookie_hdr.empty()) req << cookie_hdr;

    if (!body.empty()) {
        req << "Content-Length: " << body.size() << "\r\n";
    }

    req << "\r\n";
    if (!body.empty()) req << body;

    return req.str();
}

std::string network_manager::decompress_body(const std::string& body, const std::string& encoding) {
    if (encoding.empty() || encoding == "identity") return body;

    if (encoding == "gzip" || encoding == "x-gzip") {
        // zlib not available in current build - return raw body
        // TODO: Add gzip decompression via zlib
        static bool warned = false;
        if (!warned) { HYPERION_LOG_WARN("gzip decompression not available"); warned = true; }
        return body;
    }

    if (encoding == "deflate") {
        static bool warned = false;
        if (!warned) { HYPERION_LOG_WARN("deflate decompression not available"); warned = true; }
        return body;
    }

    if (encoding == "br") {
        HYPERION_LOG_WARN("Brotli decompression not yet supported");
        return body;
    }

    return body;
}

HttpResponse network_manager::parse_raw_response(const std::string& raw) {
    HttpResponse res;
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        res.body = raw;
        return res;
    }

    std::string header_part = raw.substr(0, header_end);
    std::string body_part = raw.substr(header_end + 4);

    std::stringstream ss(header_part);
    std::string line;

    // Parse Status Line
    if (std::getline(ss, line)) {
        size_t first_space = line.find(" ");
        if (first_space != std::string::npos) {
            std::string version = line.substr(0, first_space);
            size_t second_space = line.find(" ", first_space + 1);
            if (second_space != std::string::npos) {
                res.status_code = std::stoi(line.substr(first_space + 1, second_space - first_space - 1));
                res.status_text = line.substr(second_space + 1);
                if (!res.status_text.empty() && res.status_text.back() == '\r')
                    res.status_text.pop_back();
            }
        }
    }

    // Parse Headers
    while (std::getline(ss, line)) {
        if (line == "\r" || line.empty()) break;
        size_t colon = line.find(":");
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            value = trim_str(value);
            if (!value.empty() && value.back() == '\r') value.pop_back();
            res.headers[to_lower(key)] = value;
        }
    }

    // Handle Transfer-Encoding: chunked
    auto te_it = res.headers.find("transfer-encoding");
    if (te_it != res.headers.end() && te_it->second.find("chunked") != std::string::npos) {
        std::string decoded;
        size_t pos = 0;
        while (pos < body_part.size()) {
            size_t line_end = body_part.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            std::string size_hex = body_part.substr(pos, line_end - pos);
            long chunk_size = std::stol(size_hex, nullptr, 16);
            if (chunk_size == 0) break;
            decoded.append(body_part.substr(line_end + 2, chunk_size));
            pos = line_end + 2 + chunk_size + 2;
        }
        body_part = decoded;
    }

    // Handle Content-Encoding (gzip, deflate)
    auto ce_it = res.headers.find("content-encoding");
    if (ce_it != res.headers.end()) {
        body_part = decompress_body(body_part, ce_it->second);
    }

    res.body = body_part;
    return res;
}

HttpResponse network_manager::send_http_request(const std::string& host, int port, bool ssl,
                                                  const std::string& method, const std::string& path,
                                                  const std::map<std::string, std::string>& extra_headers,
                                                  const std::string& body, int timeout_ms) {
    HttpResponse res;

    // Check connection pool for reuse
    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_pool_mutex);
        for (auto& entry : m_connection_pool) {
            if (!entry.in_use && entry.host == host && entry.port == port && entry.ssl == ssl) {
                sock = (SOCKET)entry.socket;
                entry.in_use = true;
                break;
            }
        }
    }

    // Create new connection if needed
    bool new_connection = (sock == INVALID_SOCKET);
    if (new_connection) {
        if (ssl) {
            // HTTPS via ssl_socket
            ssl_socket* ssls = new ssl_socket();
            if (!ssls->connect(host, std::to_string(port))) {
                delete ssls;
                res.status_code = 0;
                res.body = "<html><body><h1>Connection failed</h1></body></html>";
                return res;
            }
            // Build and send request
            std::string req = build_request(method, host, path, extra_headers, body);
            ssls->send(req);

            // Read response
            std::string raw;
            char buf[65536];
            int bytes;
            while ((bytes = ssls->recv(buf, sizeof(buf))) > 0) {
                raw.append(buf, bytes);
            }
            delete ssls;
            return parse_raw_response(raw);
        } else {
            addrinfo hints = {}, *info;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &info) != 0) {
                res.status_code = 0;
                res.body = "<html><body><h1>DNS resolution failed</h1></body></html>";
                return res;
            }

            sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
            if (sock == INVALID_SOCKET) {
                freeaddrinfo(info);
                res.status_code = 0;
                res.body = "<html><body><h1>Socket creation failed</h1></body></html>";
                return res;
            }

            // Set timeout
            if (timeout_ms > 0) {
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
            }

            if (connect(sock, info->ai_addr, (int)info->ai_addrlen) == SOCKET_ERROR) {
                freeaddrinfo(info);
                closesocket(sock);
                res.status_code = 0;
                res.body = "<html><body><h1>Connection failed</h1></body></html>";
                return res;
            }
            freeaddrinfo(info);

            // Add to pool
            {
                std::lock_guard<std::mutex> lock(m_pool_mutex);
                ConnectionPoolEntry entry;
                entry.host = host;
                entry.port = port;
                entry.ssl = false;
                entry.socket = (void*)sock;
                entry.in_use = true;
                entry.last_used = std::chrono::system_clock::now();
                m_connection_pool.push_back(entry);
            }
        }
    }

    // Build and send request
    std::string req = build_request(method, host, path, extra_headers, body);
    send(sock, req.c_str(), (int)req.size(), 0);

    // Read response
    std::string raw;
    char buf[65536];
    int bytes;
    while ((bytes = recv(sock, buf, sizeof(buf), 0)) > 0) {
        raw.append(buf, bytes);
    }

    res = parse_raw_response(raw);

    // Check for Connection: close
    auto conn_it = res.headers.find("connection");
    bool keep_alive = true;
    if (conn_it != res.headers.end() && to_lower(conn_it->second).find("close") != std::string::npos) {
        keep_alive = false;
    }

    if (!keep_alive || new_connection) {
        closesocket(sock);
        // Remove from pool
        std::lock_guard<std::mutex> lock(m_pool_mutex);
        m_connection_pool.erase(
            std::remove_if(m_connection_pool.begin(), m_connection_pool.end(),
                [sock](const ConnectionPoolEntry& e) { return e.socket == (void*)sock; }),
            m_connection_pool.end());
    } else {
        // Return to pool
        std::lock_guard<std::mutex> lock(m_pool_mutex);
        for (auto& entry : m_connection_pool) {
            if (entry.socket == (void*)sock) {
                entry.in_use = false;
                entry.last_used = std::chrono::system_clock::now();
                break;
            }
        }
    }

    return res;
}

HttpResponse network_manager::fetch_internal(const std::string& url_str, int redirect_count, int timeout_ms) {
    HttpResponse res;

    std::string host, path;
    int port;
    bool ssl;
    if (!parse_url(url_str, host, port, path, ssl)) {
        res.status_code = 0;
        res.body = "<html><body><h1>Invalid URL</h1></body></html>";
        return res;
    }

    res.final_url = url_str;

    // Check cache
    std::string cache_key = url_str;
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_cache.find(cache_key);
        if (it != m_cache.end() && it->second.expires > std::chrono::system_clock::now()) {
            res.status_code = 200;
            res.body = it->second.body;
            res.headers = it->second.headers;
            res.from_cache = true;
            return res;
        }
    }

    // Build extra headers with cache validation
    std::map<std::string, std::string> extra_headers;
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_cache.find(cache_key);
        if (it != m_cache.end()) {
            if (!it->second.etag.empty())
                extra_headers["If-None-Match"] = it->second.etag;
            if (!it->second.last_modified.empty())
                extra_headers["If-Modified-Since"] = it->second.last_modified;
        }
    }

    // Add CSP headers
    extra_headers["Accept-Language"] = "en-US,en;q=0.9";
    extra_headers["Accept-Charset"] = "utf-8,iso-8859-1;q=0.5";

    res = send_http_request(host, port, ssl, "GET", path, extra_headers, "", timeout_ms);

    // Handle 304 Not Modified
    if (res.status_code == 304) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_cache.find(cache_key);
        if (it != m_cache.end()) {
            res.body = it->second.body;
            res.headers = it->second.headers;
            res.from_cache = true;
            res.status_code = 200;
        }
        return res;
    }

    // Process Set-Cookie headers (may appear multiple times)
    for (const auto& [key, val] : res.headers) {
        if (to_lower(key) == "set-cookie") {
            cookie_manager::instance().process_set_cookie(host, val);
        }
    }

    // Cache the response (for GET requests with Cache-Control)
    if (res.status_code == 200) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        CacheEntry entry;
        entry.body = res.body;
        entry.headers = res.headers;

        auto cc_it = res.headers.find("cache-control");
        if (cc_it != res.headers.end()) {
            std::string cc = to_lower(cc_it->second);
            if (cc.find("no-store") == std::string::npos && cc.find("no-cache") == std::string::npos) {
                // Parse max-age
                size_t max_age_pos = cc.find("max-age=");
                if (max_age_pos != std::string::npos) {
                    std::string age_str = cc.substr(max_age_pos + 8);
                    size_t comma = age_str.find(",");
                    if (comma != std::string::npos) age_str = age_str.substr(0, comma);
                    long max_age = std::stol(trim_str(age_str));
                    entry.expires = std::chrono::system_clock::now() + std::chrono::seconds(max_age);
                    m_cache[cache_key] = entry;
                }
            }
        }

        // Also use Expires header if present
        auto exp_it = res.headers.find("expires");
        if (exp_it != res.headers.end()) {
            // Simplified: just cache for 5 minutes if expires is present
            entry.expires = std::chrono::system_clock::now() + std::chrono::minutes(5);
            m_cache[cache_key] = entry;
        }

        // Store ETag and Last-Modified for validation
        auto etag_it = res.headers.find("etag");
        if (etag_it != res.headers.end()) entry.etag = etag_it->second;
        auto lm_it = res.headers.find("last-modified");
        if (lm_it != res.headers.end()) entry.last_modified = lm_it->second;
    }

    // Redirect following (handles 301, 302, 303, 307, 308)
    if (res.status_code >= 300 && res.status_code < 400 && redirect_count > 0) {
        auto loc_it = res.headers.find("location");
        if (loc_it != res.headers.end()) {
            std::string location = loc_it->second;
            // Handle relative redirects
            if (location.find("://") == std::string::npos) {
                if (location[0] == '/') {
                    location = (ssl ? "https://" : "http://") + host + ":" + std::to_string(port) + location;
                } else {
                    size_t last_slash = url_str.rfind("/");
                    if (last_slash != std::string::npos) {
                        location = url_str.substr(0, last_slash + 1) + location;
                    }
                }
            }

            // Determine request method for redirect
            // 303: always GET, 307/308: preserve method
            bool use_get = (res.status_code == 303 ||
                           (res.status_code == 302) ||  // Many clients do this
                           (res.status_code == 301));   // Most clients do this

            if (use_get || res.status_code == 303) {
                res = fetch_internal(location, redirect_count - 1, timeout_ms);
            } else {
                // 307/308 preserve method - just GET anyway for simplicity
                res = fetch_internal(location, redirect_count - 1, timeout_ms);
            }
        }
    }

    return res;
}

HttpResponse network_manager::fetch(const std::wstring& url, int max_redirects, int timeout_ms) {
    std::string url_str = wstring_to_utf8(url);
    return fetch_internal(url_str, max_redirects, timeout_ms);
}

std::string network_manager::perform_request(const std::wstring& url_w) {
    HttpResponse res = fetch(url_w);
    return res.body;
}

void network_manager::set_cache_limit(size_t max_bytes) {
    m_cache_max_bytes = max_bytes;
}

void network_manager::clear_cache() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    m_cache.clear();
}

void network_manager::set_max_connections(int max) {
    m_max_connections = max;
}

} // namespace hre::net
