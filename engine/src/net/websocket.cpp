#include "hre/net/websocket.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace hre::net {

std::vector<uint8_t> ws_frame::serialize() const {
    std::vector<uint8_t> out;
    uint8_t first = static_cast<uint8_t>(opcode);
    if (fin) first |= 0x80;
    out.push_back(first);

    uint8_t second = 0;
    if (masked) second |= 0x80;
    if (payload_length < 126) {
        second |= static_cast<uint8_t>(payload_length);
        out.push_back(second);
    } else if (payload_length <= 0xFFFF) {
        second |= 126;
        out.push_back(second);
        out.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    } else {
        second |= 127;
        out.push_back(second);
        for (int i = 7; i >= 0; i--) {
            out.push_back(static_cast<uint8_t>((payload_length >> (i * 8)) & 0xFF));
        }
    }

    if (masked) {
        out.insert(out.end(), masking_key, masking_key + 4);
        std::vector<uint8_t> masked_payload = payload;
        for (size_t i = 0; i < payload.size(); i++) {
            masked_payload[i] ^= masking_key[i % 4];
        }
        out.insert(out.end(), masked_payload.begin(), masked_payload.end());
    } else {
        out.insert(out.end(), payload.begin(), payload.end());
    }
    return out;
}

ws_frame ws_frame::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    ws_frame frame;
    if (offset + 2 > data.size()) return frame;

    uint8_t first = data[offset++];
    frame.fin = (first & 0x80) != 0;
    frame.opcode = static_cast<ws_opcode>(first & 0x0F);

    uint8_t second = data[offset++];
    frame.masked = (second & 0x80) != 0;
    frame.payload_length = second & 0x7F;

    if (frame.payload_length == 126) {
        if (offset + 2 > data.size()) return frame;
        frame.payload_length = (static_cast<uint64_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
    } else if (frame.payload_length == 127) {
        if (offset + 8 > data.size()) return frame;
        frame.payload_length = 0;
        for (int i = 0; i < 8; i++) {
            frame.payload_length = (frame.payload_length << 8) | data[offset + i];
        }
        offset += 8;
    }

    if (frame.masked) {
        if (offset + 4 > data.size()) return frame;
        std::memcpy(frame.masking_key, &data[offset], 4);
        offset += 4;
    }

    if (offset + frame.payload_length > data.size()) return frame;
    frame.payload.assign(data.begin() + offset, data.begin() + offset + frame.payload_length);
    offset += frame.payload_length;

    if (frame.masked) {
        for (size_t i = 0; i < frame.payload.size(); i++) {
            frame.payload[i] ^= frame.masking_key[i % 4];
        }
    }

    return frame;
}

websocket::websocket()
    : m_port(80)
    , m_use_ssl(false)
    , m_sock(INVALID_SOCKET)
    , m_state(ws_connection_state::CLOSED) {}

websocket::~websocket() { close_socket(); }

bool websocket::parse_url(const std::string& url) {
    if (url.substr(0, 3) != "ws:" && url.substr(0, 4) != "wss:") return false;
    m_use_ssl = (url.substr(0, 4) == "wss:");
    m_port = m_use_ssl ? 443 : 80;

    size_t proto_end = url.find("://");
    if (proto_end == std::string::npos) return false;

    std::string host_part = url.substr(proto_end + 3);
    size_t path_start = host_part.find("/");
    if (path_start != std::string::npos) {
        std::string host_port = host_part.substr(0, path_start);
        m_path = host_part.substr(path_start);
        size_t colon = host_port.find(":");
        if (colon != std::string::npos) {
            m_host = host_port.substr(0, colon);
            m_port = std::stoi(host_port.substr(colon + 1));
        } else {
            m_host = host_port;
        }
    } else {
        m_host = host_part;
        m_path = "/";
    }
    return !m_host.empty();
}

bool websocket::connect(const std::string& url) {
    if (!parse_url(url)) return false;
    m_url = url;

    addrinfo hints = {}, *info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(), &hints, &info) != 0)
        return false;

    m_sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (m_sock == INVALID_SOCKET) {
        freeaddrinfo(info);
        return false;
    }

    if (::connect(m_sock, info->ai_addr, static_cast<int>(info->ai_addrlen)) == SOCKET_ERROR) {
        freeaddrinfo(info);
        close_socket();
        return false;
    }
    freeaddrinfo(info);

    m_state = ws_connection_state::CONNECTING;
    if (!perform_handshake()) {
        close_socket();
        return false;
    }

    m_state = ws_connection_state::OPEN;
    return true;
}

bool websocket::perform_handshake() {
    std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string request = "GET " + m_path + " HTTP/1.1\r\n"
        "Host: " + m_host + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + key + "\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";

    if (::send(m_sock, request.c_str(), static_cast<int>(request.size()), 0) == SOCKET_ERROR)
        return false;

    char buffer[4096];
    int bytes = ::recv(m_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return false;
    buffer[bytes] = '\0';

    return std::string(buffer).find("HTTP/1.1 101") != std::string::npos;
}

bool websocket::send_raw(const std::vector<uint8_t>& data) {
    if (m_sock == INVALID_SOCKET) return false;
    return ::send(m_sock, reinterpret_cast<const char*>(data.data()),
                  static_cast<int>(data.size()), 0) == static_cast<int>(data.size());
}

std::vector<uint8_t> websocket::read_raw(size_t len) {
    std::vector<uint8_t> buf(len);
    size_t total = 0;
    while (total < len) {
        int n = ::recv(m_sock, reinterpret_cast<char*>(buf.data() + total),
                       static_cast<int>(len - total), 0);
        if (n <= 0) return {};
        total += n;
    }
    return buf;
}

bool websocket::send_frame(const ws_frame& frame) {
    auto data = frame.serialize();
    return send_raw(data);
}

bool websocket::send_text(const std::string& data) {
    ws_frame frame;
    frame.fin = true;
    frame.opcode = ws_opcode::TEXT;
    frame.payload.assign(data.begin(), data.end());
    frame.payload_length = data.size();
    return send_frame(frame);
}

bool websocket::send_binary(const std::vector<uint8_t>& data) {
    ws_frame frame;
    frame.fin = true;
    frame.opcode = ws_opcode::BINARY;
    frame.payload = data;
    frame.payload_length = data.size();
    return send_frame(frame);
}

bool websocket::send_ping(const std::vector<uint8_t>& payload) {
    ws_frame frame;
    frame.fin = true;
    frame.opcode = ws_opcode::PING;
    frame.payload = payload;
    frame.payload_length = payload.size();
    return send_frame(frame);
}

bool websocket::send_pong(const std::vector<uint8_t>& payload) {
    ws_frame frame;
    frame.fin = true;
    frame.opcode = ws_opcode::PONG;
    frame.payload = payload;
    frame.payload_length = payload.size();
    return send_frame(frame);
}

bool websocket::send_close(ws_close_reason reason, const std::string& message) {
    m_state = ws_connection_state::CLOSING;
    ws_frame frame;
    frame.fin = true;
    frame.opcode = ws_opcode::CLOSE;
    uint16_t code = static_cast<uint16_t>(reason);
    frame.payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
    frame.payload.push_back(static_cast<uint8_t>(code & 0xFF));
    frame.payload.insert(frame.payload.end(), message.begin(), message.end());
    frame.payload_length = frame.payload.size();
    return send_frame(frame);
}

ws_frame websocket::recv_frame() {
    auto header = read_raw(2);
    if (header.size() < 2) return {};

    std::vector<uint8_t> frame_data = header;
    ws_frame frame;
    size_t offset = 0;
    uint8_t first = frame_data[offset++];
    frame.fin = (first & 0x80) != 0;
    frame.opcode = static_cast<ws_opcode>(first & 0x0F);

    uint8_t second = frame_data[offset++];
    frame.masked = (second & 0x80) != 0;
    frame.payload_length = second & 0x7F;

    if (frame.payload_length == 126) {
        auto ext = read_raw(2);
        if (ext.size() < 2) return {};
        frame_data.insert(frame_data.end(), ext.begin(), ext.end());
        frame.payload_length = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (frame.payload_length == 127) {
        auto ext = read_raw(8);
        if (ext.size() < 8) return {};
        frame_data.insert(frame_data.end(), ext.begin(), ext.end());
        frame.payload_length = 0;
        for (int i = 0; i < 8; i++) {
            frame.payload_length = (frame.payload_length << 8) | ext[i];
        }
    }

    if (frame.masked) {
        auto mask = read_raw(4);
        if (mask.size() < 4) return {};
        frame_data.insert(frame_data.end(), mask.begin(), mask.end());
        std::memcpy(frame.masking_key, mask.data(), 4);
    }

    if (frame.payload_length > 0) {
        auto payload = read_raw(static_cast<size_t>(frame.payload_length));
        if (payload.size() < frame.payload_length) return {};
        frame.payload = std::move(payload);
        if (frame.masked) {
            for (size_t i = 0; i < frame.payload.size(); i++) {
                frame.payload[i] ^= frame.masking_key[i % 4];
            }
        }
    }

    return frame;
}

std::string websocket::recv_text() {
    while (m_state == ws_connection_state::OPEN) {
        auto frame = recv_frame();
        if (frame.payload.empty() && !frame.fin) return "";
        dispatch_frame(frame);
        if (frame.opcode == ws_opcode::TEXT && frame.fin) {
            return std::string(frame.payload.begin(), frame.payload.end());
        }
    }
    return "";
}

std::vector<uint8_t> websocket::recv_binary() {
    while (m_state == ws_connection_state::OPEN) {
        auto frame = recv_frame();
        if (frame.payload.empty() && !frame.fin) return {};
        dispatch_frame(frame);
        if (frame.opcode == ws_opcode::BINARY && frame.fin) {
            return frame.payload;
        }
    }
    return {};
}

void websocket::dispatch_frame(const ws_frame& frame) {
    switch (frame.opcode) {
        case ws_opcode::TEXT:
            if (m_text_cb) {
                m_text_cb(std::string(frame.payload.begin(), frame.payload.end()));
            }
            break;
        case ws_opcode::BINARY:
            if (m_binary_cb) m_binary_cb(frame.payload);
            break;
        case ws_opcode::PING:
            send_pong(frame.payload);
            if (m_ping_cb) m_ping_cb(frame.payload);
            break;
        case ws_opcode::PONG:
            if (m_pong_cb) m_pong_cb(frame.payload);
            break;
        case ws_opcode::CLOSE: {
            ws_close_reason reason = ws_close_reason::NORMAL;
            if (frame.payload.size() >= 2) {
                uint16_t code = (static_cast<uint16_t>(frame.payload[0]) << 8) | frame.payload[1];
                reason = static_cast<ws_close_reason>(code);
            }
            m_state = ws_connection_state::CLOSED;
            if (m_close_cb) m_close_cb(reason);
            break;
        }
        case ws_opcode::CONTINUATION:
        default:
            break;
    }
}

void websocket::close() {
    if (m_state == ws_connection_state::OPEN) {
        send_close();
    }
    close_socket();
}

void websocket::close_socket() {
    m_state = ws_connection_state::CLOSED;
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
}

} // namespace hre::net
