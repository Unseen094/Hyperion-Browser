#define NOMINMAX
#include "hre/net/http3/http3_client.hpp"
#include <sstream>
#include <algorithm>

namespace hre::net::http3 {

http3_client::http3_client() {}
http3_client::~http3_client() { close_all(); }

std::string http3_client::parse_host(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) return url;
    start += 3;
    size_t end = url.find(':', start);
    if (end == std::string::npos) end = url.find('/', start);
    if (end == std::string::npos) return url.substr(start);
    return url.substr(start, end - start);
}

int http3_client::parse_port(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) return 443;
    start += 3;
    size_t colon = url.find(':', start);
    if (colon == std::string::npos) return 443;
    size_t end = url.find('/', colon);
    if (end == std::string::npos) {
        int port = std::stoi(url.substr(colon + 1));
        return port;
    }
    return std::stoi(url.substr(colon + 1, end - colon - 1));
}

std::string http3_client::parse_path(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) return "/";
    start = url.find('/', start + 3);
    if (start == std::string::npos) return "/";
    size_t qmark = url.find('?', start);
    if (qmark != std::string::npos) return url.substr(start, qmark - start);
    return url.substr(start);
}

bool http3_client::is_https(const std::string& url) {
    return url.find("https://") == 0;
}

std::shared_ptr<http3_client::quic_session> http3_client::get_or_create_session(const std::string& host, int port) {
    std::string key = host + ":" + std::to_string(port);
    auto it = m_sessions.find(key);
    if (it != m_sessions.end()) return it->second;

    auto session = std::make_shared<quic_session>();
    session->host = host;
    session->port = port;
    session->conn = std::make_shared<quic_connection>();

    if (!session->conn->connect(host, port)) {
        return nullptr;
    }

    session->cc = std::make_shared<cubic_congestion>();
    m_sessions[key] = session;
    return session;
}

std::vector<uint8_t> http3_client::encode_qpack_headers(const std::vector<std::pair<std::string, std::string>>& headers) {
    std::vector<uint8_t> encoded;

    // Simplified QPACK encoding
    for (const auto& [name, value] : headers) {
        // Literal header field with name reference
        encoded.push_back(0x20 | (0x0F & static_cast<uint8_t>(name.size())));

        // Static table index for common headers
        if (name == ":method") {
            if (value == "GET") encoded.push_back(0x00);
            else if (value == "POST") encoded.push_back(0x01);
        } else if (name == ":path") {
            encoded.push_back(0x04);
        } else if (name == ":authority") {
            encoded.push_back(0x05);
        } else if (name == ":scheme") {
            if (value == "https") encoded.push_back(0x06);
            else encoded.push_back(0x07);
        } else {
            // Literal name
            encoded.push_back(0x40 | static_cast<uint8_t>(name.size()));
            encoded.insert(encoded.end(), name.begin(), name.end());
        }

        // Append value
        encoded.push_back(static_cast<uint8_t>(value.size()));
        encoded.insert(encoded.end(), value.begin(), value.end());
    }

    return encoded;
}

std::vector<std::pair<std::string, std::string>> http3_client::decode_qpack_headers(const std::vector<uint8_t>& data) {
    std::vector<std::pair<std::string, std::string>> headers;
    size_t offset = 0;

    while (offset < data.size()) {
        uint8_t b = data[offset++];
        if (b & 0x80) {
            // Indexed static
            uint32_t index = b & 0x7F;
            switch (index) {
            case 0: headers.emplace_back(":method", "GET"); break;
            case 1: headers.emplace_back(":method", "POST"); break;
            case 4: headers.emplace_back(":path", "/"); break;
            case 5: headers.emplace_back(":authority", ""); break;
            case 6: headers.emplace_back(":scheme", "https"); break;
            case 7: headers.emplace_back(":scheme", "http"); break;
            default: break;
            }
        } else if (b & 0x40) {
            // Literal with name reference to static
            uint32_t name_len = b & 0x0F;
            std::string name(reinterpret_cast<const char*>(data.data()) + offset,
                             std::min<size_t>(name_len, data.size() - offset));
            offset += name_len;
            if (offset < data.size()) {
                uint32_t val_len = data[offset++];
                std::string value(reinterpret_cast<const char*>(data.data()) + offset,
                                  std::min<size_t>(val_len, data.size() - offset));
                offset += val_len;
                headers.emplace_back(name, value);
            }
        } else if (b & 0x20) {
            // Literal with name reference (low)
            uint32_t name_len = b & 0x0F;
            if (offset + name_len <= data.size()) {
                std::string name(reinterpret_cast<const char*>(data.data()) + offset, name_len);
                offset += name_len;
                if (offset < data.size()) {
                    uint32_t val_len = data[offset++];
                    std::string value(reinterpret_cast<const char*>(data.data()) + offset,
                                      std::min<size_t>(val_len, data.size() - offset));
                    offset += val_len;
                    headers.emplace_back(name, value);
                }
            }
        }
    }

    return headers;
}

void http3_client::fetch(const std::string& url, std::function<void(const http3_response&)> callback) {
    std::string host = parse_host(url);
    int port = parse_port(url);
    std::string path = parse_path(url);

    auto session = get_or_create_session(host, port);
    if (!session || !session->conn || !session->conn->is_connected()) {
        http3_response resp;
        resp.success = false;
        resp.error_message = "Failed to establish QUIC connection to " + host;
        callback(resp);
        return;
    }

    // Build HTTP/3 request (simplified as QUIC stream data)
    std::vector<std::pair<std::string, std::string>> h3_headers;
    h3_headers.emplace_back(":method", "GET");
    h3_headers.emplace_back(":path", path);
    h3_headers.emplace_back(":authority", host);
    h3_headers.emplace_back(":scheme", "https");

    auto encoded_headers = encode_qpack_headers(h3_headers);

    // Construct HTTP/3 request frame
    std::vector<uint8_t> request_frame;
    // Push stream type (0x01 for bidirectional data)
    request_frame.push_back(0x01);
    // HEADERS frame
    request_frame.push_back(0x01); // frame type: HEADERS
    request_frame.push_back(static_cast<uint8_t>(encoded_headers.size() >> 8));
    request_frame.push_back(static_cast<uint8_t>(encoded_headers.size() & 0xFF));
    request_frame.insert(request_frame.end(), encoded_headers.begin(), encoded_headers.end());

    // DATA frame (empty for GET)
    request_frame.push_back(0x00); // frame type: DATA
    request_frame.push_back(0x00); // length high
    request_frame.push_back(0x00); // length low

    session->conn->send(request_frame.data(), request_frame.size());

    // Read response
    std::vector<uint8_t> response_buf(4096);
    int n = session->conn->recv(response_buf.data(), response_buf.size());

    http3_response resp;
    if (n > 0) {
        // Parse response frames
        size_t offset = 0;
        while (offset < static_cast<size_t>(n)) {
            if (offset >= response_buf.size()) break;
            uint8_t frame_type = response_buf[offset++];
            if (offset + 2 > response_buf.size()) break;
            uint16_t frame_len = (static_cast<uint16_t>(response_buf[offset]) << 8) |
                                 response_buf[offset + 1];
            offset += 2;

            if (frame_type == 0x01) {
                // HEADERS
                if (offset + frame_len <= response_buf.size()) {
                    std::vector<uint8_t> hdr_data(response_buf.begin() + offset,
                                                   response_buf.begin() + offset + frame_len);
                    auto decoded = decode_qpack_headers(hdr_data);
                    for (const auto& [name, value] : decoded) {
                        if (name == ":status") {
                            resp.status_code = std::stoi(value);
                        } else {
                            resp.headers[name] = value;
                        }
                    }
                }
                offset += frame_len;
            } else if (frame_type == 0x00) {
                // DATA
                if (offset + frame_len <= response_buf.size()) {
                    resp.body.insert(resp.body.end(),
                                     response_buf.begin() + offset,
                                     response_buf.begin() + offset + frame_len);
                }
                offset += frame_len;
            }
        }

        resp.success = (resp.status_code >= 200 && resp.status_code < 300);
        resp.connection_rtt = session->conn->rtt();
    } else {
        resp.success = false;
        resp.error_message = "No response received";
    }

    callback(resp);
}

void http3_client::set_congestion_control(const std::string& algo) {
    for (auto& [key, session] : m_sessions) {
        if (algo == "reno") {
            session->cc = std::make_shared<reno_congestion>();
        } else if (algo == "bbr") {
            session->cc = std::make_shared<bbr_congestion>();
        } else {
            session->cc = std::make_shared<cubic_congestion>();
        }
    }
}

bool http3_client::is_supported(const std::string& host, int port) {
    return true;
}

void http3_client::close_all() {
    for (auto& [key, session] : m_sessions) {
        if (session->conn) {
            session->conn->close(quic_error::H3_NO_ERROR);
        }
    }
    m_sessions.clear();
}

bool http3_client::is_quic_supported() {
    // Check for UDP socket support
    SOCKET test = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (test == INVALID_SOCKET) return false;
    closesocket(test);
    return true;
}

} // namespace hre::net::http3
