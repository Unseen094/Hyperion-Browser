#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <array>
#include <winsock2.h>

namespace hre::net::http3 {

struct quic_packet_header {
    bool long_header;
    uint8_t type;
    uint32_t version;
    std::array<uint8_t, 16> dst_connection_id;
    std::array<uint8_t, 16> src_connection_id;
    uint32_t packet_number;
    uint8_t flags;

    quic_packet_header()
        : long_header(true), type(0), version(1), packet_number(0), flags(0)
    { dst_connection_id.fill(0); src_connection_id.fill(0); }
};

enum class quic_error : uint16_t {
    H3_NO_ERROR = 0x0,
    INTERNAL = 0x1,
    CONNECTION_REFUSED = 0x2,
    FLOW_CONTROL = 0x3,
    STREAM_LIMIT = 0x4,
    STREAM_STATE = 0x5,
    FINAL_SIZE = 0x6,
    FRAME_ENCODING = 0x7,
    TRANSPORT_PARAMETER = 0x8,
    CONNECTION_ID_LIMIT = 0x9,
    PROTOCOL_VIOLATION = 0xA,
    INVALID_TOKEN = 0xB,
    CRYPTO_BUFFER_EXCEEDED = 0xD,
    KEY_UPDATE = 0xE
};

struct quic_transport_stats {
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t packets_lost = 0;
    double rtt_estimate = 0.0;
    uint32_t congestion_window = 0;
    uint64_t bytes_in_flight = 0;
};

class quic_transport {
public:
    quic_transport();
    ~quic_transport();

    bool init();
    void shutdown();

    int send_packet(const uint8_t* data, size_t len,
                    const sockaddr_in& dest_addr);
    int receive_packet(uint8_t* buffer, size_t max_len,
                       sockaddr_in& src_addr);

    void on_receive(std::function<void(const uint8_t*, size_t, const sockaddr_in&)> cb)
    { m_recv_cb = cb; }

    SOCKET socket() const { return m_sock; }
    const quic_transport_stats& stats() const { return m_stats; }

private:
    SOCKET m_sock;
    bool m_initialized;
    quic_transport_stats m_stats;
    std::function<void(const uint8_t*, size_t, const sockaddr_in&)> m_recv_cb;

    static const size_t MAX_PACKET_SIZE = 1472; // Ethernet MTU - IP/UDP header
};

} // namespace hre::net::http3
