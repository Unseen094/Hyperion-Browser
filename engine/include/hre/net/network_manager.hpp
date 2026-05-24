#pragma once

#include <string>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <set>

namespace hre::net {

struct HttpResponse {
    int status_code = 0;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string final_url;
    bool from_cache = false;
};

struct CacheEntry {
    std::string body;
    std::map<std::string, std::string> headers;
    std::chrono::system_clock::time_point expires;
    std::string etag;
    std::string last_modified;
};

struct ConnectionPoolEntry {
    std::string host;
    int port = 80;
    bool ssl = false;
    void* socket = nullptr;
    std::chrono::system_clock::time_point last_used;
    bool in_use = false;
};

class network_manager {
public:
    network_manager();
    ~network_manager();

    HttpResponse fetch(const std::wstring& url, int max_redirects = 5, int timeout_ms = 30000);
    std::string perform_request(const std::wstring& url);

    // Cache
    void set_cache_limit(size_t max_bytes);
    void clear_cache();

    // Keep-alive
    void set_max_connections(int max);

private:
    HttpResponse fetch_internal(const std::string& url_str, int redirect_count, int timeout_ms);
    HttpResponse send_http_request(const std::string& host, int port, bool ssl,
                                    const std::string& method, const std::string& path,
                                    const std::map<std::string, std::string>& extra_headers,
                                    const std::string& body, int timeout_ms);
    HttpResponse parse_raw_response(const std::string& raw);
    std::string decompress_body(const std::string& body, const std::string& encoding);
    std::string build_request(const std::string& method, const std::string& host,
                               const std::string& path, const std::map<std::string, std::string>& headers,
                               const std::string& body);

    // URL parsing
    static bool parse_url(const std::string& url, std::string& host, int& port,
                          std::string& path, bool& ssl);

    // Cache
    std::map<std::string, CacheEntry> m_cache;
    size_t m_cache_max_bytes = 10 * 1024 * 1024; // 10 MB
    std::mutex m_cache_mutex;

    // Connection pool
    std::vector<ConnectionPoolEntry> m_connection_pool;
    int m_max_connections = 6;
    std::mutex m_pool_mutex;

    // Cookie management
    std::string get_cookie_header(const std::string& host, bool ssl);
    void process_set_cookie(const std::string& host, const std::string& cookie_header);
};

} // namespace hre::net
