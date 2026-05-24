#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include "hre/net/http2/connection.hpp"
#include "hre/net/http2/stream.hpp"
#include "hre/net/http2/hpack.hpp"

namespace hre::net::http2 {

struct http2_response {
    int status_code = 0;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    bool success = false;
    std::string error_message;
};

class http2_client {
public:
    http2_client();
    ~http2_client();

    void fetch(const std::string& url,
               std::function<void(const http2_response&)> callback);
    void fetch_async(const std::string& url,
                     std::function<void(const http2_response&)> callback);

    bool is_supported(const std::string& host, int port);
    void close_all();

private:
    std::map<std::string, std::shared_ptr<http2_connection>> m_connections;

    std::string parse_host(const std::string& url);
    int parse_port(const std::string& url);
    std::string parse_path(const std::string& url);
    bool is_https(const std::string& url);

    std::shared_ptr<http2_connection> get_or_create_connection(const std::string& host, int port);
    void build_request_headers(const std::string& method, const std::string& path,
                               const std::string& host, std::vector<header_entry>& headers);
};

} // namespace hre::net::http2
