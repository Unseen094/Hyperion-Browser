#define NOMINMAX
#include "hre/net/http3/quic_connection.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <algorithm>
#include <thread>
#include <random>

#pragma comment(lib, "ws2_32.lib")

namespace hre::net::http3 {

static std::vector<uint8_t> generate_random_bytes(size_t len) {
    static std::mt19937 rng(std::random_device{}());
    std::vector<uint8_t> buf(len);
    for (auto& b : buf) b = static_cast<uint8_t>(rng() & 0xFF);
    return buf;
}

quic_connection::quic_connection()
    : m_state(connection_state::INITIAL)
    , m_port(443)
    , m_sock(INVALID_SOCKET)
    , m_next_packet_number(0)
    , m_rtt(0)
    , m_max_streams_bidi(100)
    , m_max_streams_uni(100)
    , m_max_data(1048576)
{}

quic_connection::~quic_connection() {
    close(quic_error::H3_NO_ERROR);
}

bool quic_connection::connect(const std::string& host, int port) {
    m_host = host;
    m_port = port;

    addrinfo hints = {}, *info;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &info) != 0)
        return false;

    m_sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (m_sock == INVALID_SOCKET) {
        freeaddrinfo(info);
        return false;
    }

    // Set non-blocking
    u_long mode = 1;
    ioctlsocket(m_sock, FIONBIO, &mode);

    if (::connect(m_sock, info->ai_addr, (int)info->ai_addrlen) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            freeaddrinfo(info);
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            return false;
        }
    }

    m_peer_addr = *reinterpret_cast<sockaddr_in*>(info->ai_addr);
    freeaddrinfo(info);

    m_state = connection_state::HANDSHAKE;

    // Generate connection IDs
    generate_connection_id(m_original_dst_cid);
    generate_connection_id(m_initial_scid);
    m_current_dcid = m_original_dst_cid;
    m_current_scid = m_initial_scid;

    // Derive initial keys and send first packet
    m_handshake_start = std::chrono::steady_clock::now();

    if (!send_initial_packet()) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    return true;
}

bool quic_connection::handshake_0rtt() {
    // 0-RTT handshake for session resumption
    if (m_state != connection_state::INITIAL) return false;
    m_state = connection_state::HANDSHAKE;
    return send_initial_packet();
}

void quic_connection::close(quic_error ec) {
    connection_state prev = m_state;
    m_state = connection_state::CLOSED;

    if (prev == connection_state::ESTABLISHED && m_sock != INVALID_SOCKET) {
        // Send CONNECTION_CLOSE frame
        std::vector<uint8_t> close_pkt;
        close_pkt.push_back(0x1c); // CONNECTION_CLOSE
        close_pkt.push_back(static_cast<uint8_t>(ec));
        send(close_pkt.data(), close_pkt.size());
    }

    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    if (m_disconnected_cb) m_disconnected_cb(ec);
}

bool quic_connection::migrate(const sockaddr_in& new_addr) {
    m_peer_addr = new_addr;
    // Send path migration frame
    return true;
}

bool quic_connection::send(const uint8_t* data, size_t len) {
    if (m_sock == INVALID_SOCKET || m_state != connection_state::ESTABLISHED)
        return false;

    // Build QUIC packet with 1-RTT protection
    std::vector<uint8_t> packet;
    packet.push_back(0x40 | (m_current_dcid.length & 0x0F)); // short header
    packet.insert(packet.end(), m_current_dcid.data.begin(), m_current_dcid.data.begin() + m_current_dcid.length);

    // Packet number
    uint32_t pn = m_next_packet_number++;
    packet.push_back(static_cast<uint8_t>(pn >> 24));
    packet.push_back(static_cast<uint8_t>(pn >> 16));
    packet.push_back(static_cast<uint8_t>(pn >> 8));
    packet.push_back(static_cast<uint8_t>(pn));

    // Payload (simplified - no encryption)
    packet.insert(packet.end(), data, data + len);

    // Add AEAD tag placeholder
    packet.insert(packet.end(), 16, 0);

    int sent = ::sendto(m_sock, reinterpret_cast<const char*>(packet.data()),
                        static_cast<int>(packet.size()), 0,
                        reinterpret_cast<sockaddr*>(&m_peer_addr), sizeof(m_peer_addr));
    return sent == static_cast<int>(packet.size());
}

int quic_connection::recv(uint8_t* buffer, size_t max_len) {
    if (m_sock == INVALID_SOCKET) return -1;

    sockaddr_in from;
    int from_len = sizeof(from);
    int n = ::recvfrom(m_sock, reinterpret_cast<char*>(buffer),
                       static_cast<int>(max_len), 0,
                       reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n > 0) {
        process_packet(buffer, n, from);
    }
    return n;
}

void quic_connection::process_packet(const uint8_t* data, size_t len, const sockaddr_in& src_addr) {
    if (len < 1) return;

    uint8_t first_byte = data[0];

    if (first_byte & 0x80) {
        // Long header
        uint8_t type = (first_byte >> 4) & 0x03;
        if (type == 0x00) {
            process_initial_packet(data, len);
        } else if (type == 0x02) {
            process_handshake_packet(data, len);
        }
    } else {
        // Short header (1-RTT)
        process_1rtt_packet(data, len);
    }
}

bool quic_connection::derive_initial_keys(const std::array<uint8_t, 16>& dst_cid) {
    // Simplified key derivation
    m_keys.client_initial_secret = generate_random_bytes(32);
    m_keys.server_initial_secret = generate_random_bytes(32);
    m_keys.initial_derived = true;
    return true;
}

bool quic_connection::send_initial_packet() {
    if (!derive_initial_keys(m_original_dst_cid.data)) return false;

    // Build Initial packet
    std::vector<uint8_t> packet;

    // Long header: type=Initial (0x00)
    packet.push_back(0xC0); // Initial with fixed bit

    // Version (QUIC v1)
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x01);

    // Destination Connection ID
    packet.push_back(m_current_dcid.length);
    packet.insert(packet.end(), m_current_dcid.data.begin(),
                  m_current_dcid.data.begin() + m_current_dcid.length);

    // Source Connection ID
    packet.push_back(m_current_scid.length);
    packet.insert(packet.end(), m_current_scid.data.begin(),
                  m_current_scid.data.begin() + m_current_scid.length);

    // Token length (0)
    packet.push_back(0x00);

    // Payload length (placeholder)
    packet.push_back(0x00);

    // Packet number
    uint32_t pn = m_next_packet_number++;
    packet.push_back(static_cast<uint8_t>(pn >> 24));
    packet.push_back(static_cast<uint8_t>(pn >> 16));
    packet.push_back(static_cast<uint8_t>(pn >> 8));
    packet.push_back(static_cast<uint8_t>(pn));

    // CRYPTO frame for ClientHello
    // Frame type: CRYPTO
    packet.push_back(0x06);

    // Offset (0)
    packet.push_back(0x00);

    // Length (simplified: empty hello)
    packet.push_back(0x00);

    // Add AEAD tag
    packet.insert(packet.end(), 16, 0);

    // Update payload length
    size_t payload_len = packet.size() - 1; // minus first byte
    if (payload_len > 0x3FFF) return false;
    packet[22] = static_cast<uint8_t>((payload_len >> 8) & 0xFF);
    packet[23] = static_cast<uint8_t>(payload_len & 0xFF);

    // Fix packet number offset
    // (simplified - just send it)

    int sent = ::sendto(m_sock, reinterpret_cast<const char*>(packet.data()),
                        static_cast<int>(packet.size()), 0,
                        reinterpret_cast<sockaddr*>(&m_peer_addr), sizeof(m_peer_addr));
    return sent > 0;
}

bool quic_connection::send_handshake_packet() {
    // Similar to initial but with Handshake type
    std::vector<uint8_t> packet;
    packet.push_back(0xE0); // Handshake type

    // Version
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x01);

    // DCID
    packet.push_back(m_current_dcid.length);
    packet.insert(packet.end(), m_current_dcid.data.begin(),
                  m_current_dcid.data.begin() + m_current_dcid.length);

    // SCID
    packet.push_back(m_current_scid.length);
    packet.insert(packet.end(), m_current_scid.data.begin(),
                  m_current_scid.data.begin() + m_current_scid.length);

    // Payload length
    packet.push_back(0x00);

    // Packet number
    uint32_t pn = m_next_packet_number++;
    packet.push_back(static_cast<uint8_t>(pn >> 24));
    packet.push_back(static_cast<uint8_t>(pn >> 16));
    packet.push_back(static_cast<uint8_t>(pn >> 8));
    packet.push_back(static_cast<uint8_t>(pn));

    // CRYPTO frame with ServerHello response processing
    packet.push_back(0x06); // CRYPTO frame type
    packet.push_back(0x00); // offset
    packet.push_back(0x00); // length (simplified)
    packet.insert(packet.end(), 16, 0);

    int sent = ::sendto(m_sock, reinterpret_cast<const char*>(packet.data()),
                        static_cast<int>(packet.size()), 0,
                        reinterpret_cast<sockaddr*>(&m_peer_addr), sizeof(m_peer_addr));
    return sent > 0;
}

void quic_connection::handle_handshake_complete() {
    m_state = connection_state::ESTABLISHED;
    m_rtt = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - m_handshake_start).count());

    if (m_connected_cb) m_connected_cb();
}

void quic_connection::process_initial_packet(const uint8_t* data, size_t len) {
    if (len < 25) return;

    // Parse response to our initial packet
    // Extract server config from CRYPTO frame
    size_t offset = 24; // skip long header
    uint32_t pn = (static_cast<uint32_t>(data[offset]) << 24) |
                  (static_cast<uint32_t>(data[offset + 1]) << 16) |
                  (static_cast<uint32_t>(data[offset + 2]) << 8) |
                  static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    // Parse frames
    while (offset < len - 16) { // -16 for AEAD tag
        uint8_t frame_type = data[offset++];
        if (frame_type == 0x06) {
            // CRYPTO frame
            // Skip offset (varint)
            offset += 1;
            // Read length
            size_t crypto_len = data[offset++];
            if (offset + crypto_len <= len - 16) {
                // CRYPTO data received (simplified - skip processing)
                offset += crypto_len;
            }
        } else if (frame_type == 0x00) {
            // PADDING - skip
        } else {
            break;
        }
    }

    // Send handshake packet
    m_keys.client_handshake_secret = generate_random_bytes(32);
    m_keys.handshake_derived = true;
    send_handshake_packet();
}

void quic_connection::process_handshake_packet(const uint8_t* data, size_t len) {
    // Process HANDSHAKE response
    m_keys.client_1rtt_secret = generate_random_bytes(32);
    m_keys.server_1rtt_secret = generate_random_bytes(32);
    m_keys.one_rtt_derived = true;

    handle_handshake_complete();
}

void quic_connection::process_1rtt_packet(const uint8_t* data, size_t len) {
    if (m_data_cb) {
        // Skip short header + DCID + packet number
        size_t offset = 1 + m_current_dcid.length + 4;
        if (offset < len) {
            m_data_cb(data + offset, len - offset - 16); // -16 AEAD tag
        }
    }
}

void quic_connection::update_key_phase() {
    // Rotate keys (simplified - regenerate)
    m_keys.client_1rtt_secret = generate_random_bytes(32);
    m_keys.server_1rtt_secret = generate_random_bytes(32);
}

void quic_connection::generate_connection_id(connection_id& cid) {
    cid.data = {};
    cid.length = 8;
    auto bytes = generate_random_bytes(8);
    std::copy(bytes.begin(), bytes.end(), cid.data.begin());
}

} // namespace hre::net::http3
