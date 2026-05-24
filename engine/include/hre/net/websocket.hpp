#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <winsock2.h>

namespace hre::net {

enum class ws_opcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

enum class ws_connection_state {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

enum class ws_close_reason : uint16_t {
    NORMAL = 1000,
    GOING_AWAY = 1001,
    PROTOCOL_ERROR = 1002,
    UNSUPPORTED_DATA = 1003,
    INVALID_FRAME = 1007,
    POLICY_VIOLATION = 1008,
    MESSAGE_TOO_BIG = 1009,
    INTERNAL_ERROR = 1011
};

struct ws_frame {
    bool fin;
    ws_opcode opcode;
    bool masked;
    uint64_t payload_length;
    uint8_t masking_key[4];
    std::vector<uint8_t> payload;

    ws_frame() : fin(true), opcode(ws_opcode::TEXT), masked(false), payload_length(0) {
        masking_key[0] = 0; masking_key[1] = 0; masking_key[2] = 0; masking_key[3] = 0;
    }

    std::vector<uint8_t> serialize() const;
    static ws_frame deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

class websocket {
public:
    websocket();
    ~websocket();

    bool connect(const std::string& url);

    bool send(const std::string& data) { return send_text(data); }
    bool send_text(const std::string& data);
    bool send_binary(const std::vector<uint8_t>& data);
    bool send_ping(const std::vector<uint8_t>& payload = {});
    bool send_pong(const std::vector<uint8_t>& payload = {});
    bool send_close(ws_close_reason reason = ws_close_reason::NORMAL, const std::string& message = "");
    bool send_frame(const ws_frame& frame);

    ws_frame recv_frame();
    std::string recv_text();
    std::vector<uint8_t> recv_binary();

    void close();

    ws_connection_state state() const { return m_state; }
    bool is_connected() const { return m_state == ws_connection_state::OPEN; }
    bool is_connecting() const { return m_state == ws_connection_state::CONNECTING; }

    void on_text(std::function<void(const std::string&)> cb) { m_text_cb = cb; }
    void on_binary(std::function<void(const std::vector<uint8_t>&)> cb) { m_binary_cb = cb; }
    void on_ping(std::function<void(const std::vector<uint8_t>&)> cb) { m_ping_cb = cb; }
    void on_pong(std::function<void(const std::vector<uint8_t>&)> cb) { m_pong_cb = cb; }
    void on_close(std::function<void(ws_close_reason)> cb) { m_close_cb = cb; }
    void on_error(std::function<void(const std::string&)> cb) { m_error_cb = cb; }

private:
    std::string m_url;
    std::string m_host;
    std::string m_path;
    int m_port;
    bool m_use_ssl;
    SOCKET m_sock;
    ws_connection_state m_state;

    std::function<void(const std::string&)> m_text_cb;
    std::function<void(const std::vector<uint8_t>&)> m_binary_cb;
    std::function<void(const std::vector<uint8_t>&)> m_ping_cb;
    std::function<void(const std::vector<uint8_t>&)> m_pong_cb;
    std::function<void(ws_close_reason)> m_close_cb;
    std::function<void(const std::string&)> m_error_cb;

    bool parse_url(const std::string& url);
    bool send_raw(const std::vector<uint8_t>& data);
    std::vector<uint8_t> read_raw(size_t len);
    bool perform_handshake();
    void dispatch_frame(const ws_frame& frame);
    void close_socket();
};

} // namespace hre::net
