#include "hre/net/http2/client.hpp"
#include <sstream>
#include <algorithm>

namespace hre::net::http2 {

http2_client::http2_client() {}
http2_client::~http2_client() { close_all(); }

void http2_client::fetch(const std::string& url, std::function<void(const http2_response&)> callback) {
    std::string host = parse_host(url);
    int port = parse_port(url);
    std::string path = parse_path(url);

    auto conn = get_or_create_connection(host, port);
    if (!conn) {
        http2_response resp;
        resp.success = false;
        resp.error_message = "Failed to connect to " + host;
        callback(resp);
        return;
    }

    std::vector<header_entry> headers;
    build_request_headers("GET", path, host, headers);

    uint32_t stream_id = conn->open_stream(headers, true);
    if (stream_id == 0) {
        http2_response resp;
        resp.success = false;
        resp.error_message = "Failed to open stream";
        callback(resp);
        return;
    }

    auto stream = conn->get_stream(stream_id);

    stream->on_headers([callback](const std::vector<header_entry>& hdrs) {
        http2_response resp;
        resp.success = true;
        for (const auto& h : hdrs) {
            if (h.name == ":status") {
                resp.status_code = std::stoi(h.value);
            } else {
                resp.headers[h.name] = h.value;
            }
        }
        if (hdrs.empty()) callback(resp);
    });

    stream->on_data([stream, callback](const std::vector<uint8_t>& data) {
        if (stream->end_stream_received()) {
            http2_response resp;
            resp.success = true;
            for (const auto& h : stream->response_headers()) {
                if (h.name == ":status") {
                    resp.status_code = std::stoi(h.value);
                } else {
                    resp.headers[h.name] = h.value;
                }
            }
            resp.body = stream->response_body();
            callback(resp);
        }
    });

    stream->on_close([this, callback, stream](error_code ec) {
        if (!stream->end_stream_received()) {
            http2_response resp;
            resp.success = false;
            resp.error_message = "Stream closed with error: " + std::to_string(static_cast<uint32_t>(ec));
            callback(resp);
        }
    });

    while (conn->is_connected()) {
        conn->poll();
        if (stream->state() == stream_state::CLOSED ||
            stream->state() == stream_state::HALF_CLOSED_REMOTE) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void http2_client::fetch_async(const std::string& url, std::function<void(const http2_response&)> callback) {
    std::thread([this, url, callback]() {
        fetch(url, callback);
    }).detach();
}

bool http2_client::is_supported(const std::string& host, int port) {
    auto conn = get_or_create_connection(host, port);
    return conn != nullptr && conn->is_connected();
}

void http2_client::close_all() {
    m_connections.clear();
}

std::string http2_client::parse_host(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    size_t end = url.find('/', start);
    size_t port_colon = url.find(':', start);
    if (port_colon != std::string::npos && port_colon < end)
        end = port_colon;
    if (end == std::string::npos) end = url.size();
    return url.substr(start, end - start);
}

int http2_client::parse_port(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    size_t colon = url.find(':', start);
    if (colon == std::string::npos) {
        return is_https(url) ? 443 : 80;
    }
    size_t slash = url.find('/', colon);
    std::string port_str = url.substr(colon + 1, slash - colon - 1);
    return std::stoi(port_str);
}

std::string http2_client::parse_path(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    size_t slash = url.find('/', start);
    if (slash == std::string::npos) return "/";
    return url.substr(slash);
}

bool http2_client::is_https(const std::string& url) {
    return url.find("https://") == 0;
}

std::shared_ptr<http2_connection> http2_client::get_or_create_connection(const std::string& host, int port) {
    std::string key = host + ":" + std::to_string(port);
    auto it = m_connections.find(key);
    if (it != m_connections.end() && it->second->is_connected()) {
        return it->second;
    }

    auto conn = std::make_shared<http2_connection>();
    if (conn->connect(host, port)) {
        m_connections[key] = conn;
        return conn;
    }
    return nullptr;
}

void http2_client::build_request_headers(const std::string& method, const std::string& path,
                                          const std::string& host, std::vector<header_entry>& headers) {
    headers.emplace_back(":method", method);
    headers.emplace_back(":path", path);
    headers.emplace_back(":scheme", is_https(host) ? "https" : "http");
    headers.emplace_back(":authority", host);
    headers.emplace_back("user-agent", "HyperionBrowser/0.1.0");
    headers.emplace_back("accept", "*/*");
}

} // namespace hre::net::http2
