#include "hre/net/http3/quic_transport.hpp"
#include <cstring>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace hre::net::http3 {

quic_transport::quic_transport()
    : m_sock(INVALID_SOCKET)
    , m_initialized(false)
{}

quic_transport::~quic_transport() {
    shutdown();
}

bool quic_transport::init() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        return false;

    m_sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    u_long nonblock = 1;
    ioctlsocket(m_sock, FIONBIO, &nonblock);

    int recv_buf = 256 * 1024;
    setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&recv_buf), sizeof(recv_buf));

    m_initialized = true;
    return true;
}

void quic_transport::shutdown() {
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    if (m_initialized) {
        WSACleanup();
        m_initialized = false;
    }
}

int quic_transport::send_packet(const uint8_t* data, size_t len, const sockaddr_in& dest_addr) {
    if (m_sock == INVALID_SOCKET || len > MAX_PACKET_SIZE)
        return -1;

    int sent = sendto(m_sock, reinterpret_cast<const char*>(data),
                      static_cast<int>(len), 0,
                      reinterpret_cast<const sockaddr*>(&dest_addr),
                      sizeof(dest_addr));
    if (sent > 0) {
        m_stats.packets_sent++;
        m_stats.bytes_sent += sent;
    }
    return sent;
}

int quic_transport::receive_packet(uint8_t* buffer, size_t max_len, sockaddr_in& src_addr) {
    if (m_sock == INVALID_SOCKET) return -1;

    int addr_len = sizeof(src_addr);
    int received = recvfrom(m_sock, reinterpret_cast<char*>(buffer),
                            static_cast<int>(max_len), 0,
                            reinterpret_cast<sockaddr*>(&src_addr),
                            &addr_len);
    if (received > 0) {
        m_stats.packets_received++;
        m_stats.bytes_received += received;
        if (m_recv_cb) {
            m_recv_cb(buffer, received, src_addr);
        }
    }
    return received;
}

} // namespace hre::net::http3
