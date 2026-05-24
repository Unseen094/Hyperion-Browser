#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <array>
#include <map>
#include <chrono>
#include <winsock2.h>
#include "hre/net/http3/quic_transport.hpp"

namespace hre::net::http3 {

struct connection_id {
    std::array<uint8_t, 16> data;
    uint8_t length;

    connection_id() : length(0) { data.fill(0); }
    bool operator<(const connection_id& other) const { return data < other.data; }
};

enum class connection_state {
    INITIAL,
    HANDSHAKE,
    ESTABLISHED,
    CLOSING,
    DRAINING,
    CLOSED
};

struct crypto_keys {
    std::vector<uint8_t> client_initial_secret;
    std::vector<uint8_t> server_initial_secret;
    std::vector<uint8_t> client_handshake_secret;
    std::vector<uint8_t> server_handshake_secret;
    std::vector<uint8_t> client_1rtt_secret;
    std::vector<uint8_t> server_1rtt_secret;

    bool initial_derived = false;
    bool handshake_derived = false;
    bool one_rtt_derived = false;
};

class quic_connection {
public:
    quic_connection();
    ~quic_connection();

    bool connect(const std::string& host, int port);
    bool handshake_0rtt();
    void close(quic_error ec = quic_error::H3_NO_ERROR);

    bool migrate(const sockaddr_in& new_addr);
    bool is_connected() const { return m_state == connection_state::ESTABLISHED; }
    connection_state state() const { return m_state; }

    bool send(const uint8_t* data, size_t len);
    int recv(uint8_t* buffer, size_t max_len);

    void on_connected(std::function<void()> cb) { m_connected_cb = cb; }
    void on_data(std::function<void(const uint8_t*, size_t)> cb) { m_data_cb = cb; }
    void on_disconnected(std::function<void(quic_error)> cb) { m_disconnected_cb = cb; }

    uint32_t rtt() const { return m_rtt; }
    const connection_id& original_dst_cid() const { return m_original_dst_cid; }
    const connection_id& initial_scid() const { return m_initial_scid; }

    void process_packet(const uint8_t* data, size_t len, const sockaddr_in& src_addr);

private:
    connection_state m_state;
    std::string m_host;
    int m_port;

    quic_transport m_transport;
    sockaddr_in m_peer_addr;

    SOCKET m_sock;
    connection_id m_original_dst_cid;
    connection_id m_initial_scid;
    connection_id m_current_dcid;
    connection_id m_current_scid;

    crypto_keys m_keys;
    uint32_t m_next_packet_number;
    uint32_t m_rtt;
    std::chrono::steady_clock::time_point m_handshake_start;

    uint64_t m_max_streams_bidi;
    uint64_t m_max_streams_uni;
    uint64_t m_max_data;

    std::function<void()> m_connected_cb;
    std::function<void(const uint8_t*, size_t)> m_data_cb;
    std::function<void(quic_error)> m_disconnected_cb;

    bool derive_initial_keys(const std::array<uint8_t, 16>& dst_cid);
    bool send_initial_packet();
    bool send_handshake_packet();
    void handle_handshake_complete();
    void process_initial_packet(const uint8_t* data, size_t len);
    void process_handshake_packet(const uint8_t* data, size_t len);
    void process_1rtt_packet(const uint8_t* data, size_t len);
    void generate_connection_id(connection_id& cid);
    void update_key_phase();
};

} // namespace hre::net::http3
