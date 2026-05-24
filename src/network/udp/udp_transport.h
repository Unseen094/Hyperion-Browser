#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <unordered_map>
#include <string>
#include <array>

namespace hyperion::network::udp {

// UDP socket interface
class UdpSocket {
public:
    virtual ~UdpSocket() = default;
    
    virtual void bind(const std::string& address, uint16_t port) = 0;
    virtual void send(const std::string& address, uint16_t port, const std::vector<uint8_t>& data) = 0;
    virtual std::vector<uint8_t> receive() = 0;
    virtual std::pair<std::string, uint16_t> receive_from() = 0;
    virtual bool is_connected() const = 0;
    
    virtual void close() = 0;
    virtual bool has_data() const = 0;
};

// UDP transport with 0-RTT support
class UdpTransport {
public:
    // Create UDP transport instance
    static std::unique_ptr<UdpSocket> create_socket() {
        // Platform-specific socket creation
        #ifdef _WIN32
        return std::make_unique<WindowsUdpSocket>();
        #elif __linux__
        return std::make_unique<LinuxUdpSocket>();
        #else
        return std::make_unique<GenericUdpSocket>();
        #endif
    }
    
    // UDP-based transport with 0-RTT connection establishment
    // 0-RTT allows sending data on first packet without handshake
    class ZeroRttConnection {
    public:
        explicit ZeroRttConnection(UdpSocket& socket) : socket_(socket) {}
        
        // Send data with connection identifier (simulated 0-RTT)
        void send_with_connection_id(const std::string& address, uint16_t port, 
                                  const std::vector<uint8_t>& data) {
            // In real implementation, this would generate or have a pre-shared connection ID
            // For Phase 14, we simulate 0-RTT by including connection context
            std::vector<uint8_t> packet = data;
            
            // Add connection context at the end
            packet.push_back(0x01); // Magic byte for 0-RTT
            packet.push_back(static_cast<uint8_t>(connection_id_));
            packet.push_back(static_cast<uint8_t>(connection_id_ >> 8));
            packet.push_back(static_cast<uint8_t>(connection_id_ >> 16));
            packet.push_back(static_cast<uint8_t>(connection_id_ >> 24));
            
            socket_.send(address, port, packet);
            connection_id_++;
        }
        
        // Binary protocol-aware send for QUIC-like packets
        void send_packet(const std::string& address, uint16_t port, 
                      const std::vector<uint8_t>& packet) {
            // QUIC packets have connection ID + packet number in long header format
            // First 4 bytes: connection ID prefix
            std::vector<uint8_t> quic_packet;
            quic_packet.reserve(4 + packet.size());
            
            // Simulate connection ID - in real QUIC, CID is 64-160 bits
            quic_packet.push_back(0xDE);
            quic_packet.push_back(0xAD);
            quic_packet.push_back(0xBE);
            quic_packet.push_back(0xEF);
            
            // Add packet number (4 bytes) - QUIC uses variable-length encoding
            quic_packet.push_back(0x00);
            quic_packet.push_back(0x00);
            quic_packet.push_back(0x00);
            quic_packet.push_back(static_cast<uint8_t>(packet_counter_++));
            
            quic_packet.insert(quic_packet.end(), packet.begin(), packet.end());
            socket_.send(address, port, quic_packet);
        }
        
        uint32_t connection_id() const { return connection_id_; }
        
        // QUIC transport over UDP
        void send_quic_packet(const std::string& address, uint16_t port, const std::vector<uint8_t>& data) {
            // QUIC packets have special format with connection ID prefix
            std::vector<uint8_t> quic_packet;
            
            // 4-byte connection ID prefix (simulated)
            quic_packet.push_back(0xDE);
            quic_packet.push_back(0xAD);
            quic_packet.push_back(0xBE);
            quic_packet.push_back(0xEF);
            
            // Packet number (4 bytes)
            quic_packet.push_back(0x00);
            quic_packet.push_back(0x00);
            quic_packet.push_back(0x00);
            quic_packet.push_back(static_cast<uint8_t>(packet_counter_++));
            
            quic_packet.insert(quic_packet.end(), data.begin(), data.end());
            socket_.send(address, port, quic_packet);
        }
        
        // Receive QUIC packet with connection context
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> receive_all_quic_packets() {
            std::vector<std::pair<uint32_t, std::vector<uint8_t>>> results;
            
            while (socket_.has_data()) {
                try {
                    auto [address, port] = socket_.receive_from();
                    auto data = socket_.receive();
                    auto cleaned = QuicPacket::parse_packet(data);
                    if (!cleaned.empty()) {
                        results.emplace_back(0, cleaned); // 0 is placeholder for connection ID
                    }
                } catch (...) {
                    // Ignore malformed packets in continuous receive
                }
            }
            
            return results;
        }

    private:
        UdpSocket& socket_;
        uint32_t connection_id_ = 0x12345678;
        uint32_t packet_counter_ = 0;
    };
    
    // UDP datagram buffer management
    class DatagramBuffer {
    public:
        void add_packet(uint32_t connection_id, uint32_t packet_number, 
                      const std::vector<uint8_t>& data) {
            std::lock_guard<std::mutex> lock(mutex_);
            BufferKey key{connection_id, packet_number};
            buffers_[key] = data;
        }
        
        std::vector<uint8_t> get_packet(uint32_t connection_id, uint32_t packet_number) {
            std::lock_guard<std::mutex> lock(mutex_);
            BufferKey key{connection_id, packet_number};
            auto it = buffers_.find(key);
            if (it != buffers_.end()) {
                auto data = it->second;
                buffers_.erase(it);
                return data;
            }
            return {};
        }
        
        bool has_packet(uint32_t connection_id, uint32_t packet_number) {
            std::lock_guard<std::mutex> lock(mutex_);
            return buffers_.find(BufferKey{connection_id, packet_number}) != buffers_.end();
        }
        
        void cleanup_old_packets(uint32_t max_packet_number) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = buffers_.begin();
            while (it != buffers_.end()) {
                if (it->first.packet_number < max_packet_number) {
                    buffers_.erase(it++);
                } else {
                    ++it;
                }
            }
        }

    private:
        struct BufferKey {
            uint32_t connection_id;
            uint32_t packet_number;
            
            bool operator==(const BufferKey& other) const {
                return connection_id == other.connection_id && 
                       packet_number == other.packet_number;
            }
        };
        
        struct BufferKeyHash {
            size_t operator()(const BufferKey& key) const {
                return std::hash<uint32_t>()(key.connection_id) ^
                       std::hash<uint32_t>()(key.packet_number);
            }
        };
        
        std::unordered_map<BufferKey, std::vector<uint8_t>, BufferKeyHash> buffers_;
        mutable std::mutex mutex_;
    };
};

// Platform-specific implementations
#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

class WindowsUdpSocket : public UdpSocket {
public:
    WindowsUdpSocket() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    
    ~WindowsUdpSocket() {
        close();
        WSACleanup();
    }
    
    void bind(const std::string& address, uint16_t port) override {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            throw std::runtime_error("Failed to create UDP socket");
        }
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        InetPtonA(AF_INET, address.c_str(), &addr.sin_addr);
        
        if (::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw std::runtime_error("Failed to bind UDP socket");
        }
        
        is_bound_ = true;
    }
    
    void send(const std::string& address, uint16_t port, const std::vector<uint8_t>& data) override {
        if (!is_bound_) {
            throw std::runtime_error("Socket not bound");
        }
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        InetPtonA(AF_INET, address.c_str(), &addr.sin_addr);
        
        int result = ::sendto(socket_, reinterpret_cast<const char*>(data.data()), 
                           static_cast<int>(data.size()), 0, 
                           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        
        if (result == SOCKET_ERROR) {
            throw std::runtime_error("Failed to send UDP packet");
        }
    }
    
    std::vector<uint8_t> receive() override {
        if (!is_bound_) {
            throw std::runtime_error("Socket not bound");
        }
        
        // Peek message size first
        u_long size = 0;
        ioctlsocket(socket_, FIONREAD, &size);
        
        if (size == 0) {
            return {};
        }
        
        std::vector<uint8_t> buffer(size);
        int result = recv(socket_, reinterpret_cast<char*>(buffer.data()), size, MSG_PEEK);
        
        if (result == SOCKET_ERROR) {
            throw std::runtime_error("Failed to peek packet size");
        }
        
        buffer.resize(result);
        sockaddr_in from{};
        int from_len = sizeof(from);
        result = recvfrom(socket_, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0,
                        reinterpret_cast<sockaddr*>(&from), &from_len);
        
        if (result == SOCKET_ERROR) {
            throw std::runtime_error("Failed to receive packet");
        }
        
        buffer.resize(result);
        return buffer;
    }
    
    std::pair<std::string, uint16_t> receive_from() override {
        sockaddr_in from{};
        int from_len = sizeof(from);
        char buffer[65536];
        int result = recvfrom(socket_, buffer, sizeof(buffer), 0,
                            reinterpret_cast<sockaddr*>(&from), &from_len);
        
        if (result == SOCKET_ERROR) {
            throw std::runtime_error("Failed to receive packet");
        }
        
        char ip_str[INET_ADDRSTRLEN];
        InetNtopA(AF_INET, &from.sin_addr, ip_str, sizeof(ip_str));
        return {std::string(ip_str), ntohs(from.sin_port)};
    }
    
    bool is_connected() const override {
        return socket_ != INVALID_SOCKET && is_bound_;
    }
    
    void close() override {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        is_bound_ = false;
    }
    
    bool has_data() const override {
        u_long size = 0;
        ioctlsocket(socket_, FIONREAD, &size);
        return size > 0;
    }

private:
    SOCKET socket_ = INVALID_SOCKET;
    bool is_bound_ = false;
};

#elif __linux__

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

class LinuxUdpSocket : public UdpSocket {
public:
    LinuxUdpSocket() : socket_(socket(AF_INET, SOCK_DGRAM, 0)) {}
    
    ~LinuxUdpSocket() {
        close();
    }
    
    void bind(const std::string& address, uint16_t port) override {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &addr.sin_addr);
        
        if (::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            ::close(socket_);
            socket_ = -1;
            throw std::runtime_error("Failed to bind UDP socket");
        }
        
        is_bound_ = true;
    }
    
    void send(const std::string& address, uint16_t port, const std::vector<uint8_t>& data) override {
        if (socket_ == -1) {
            throw std::runtime_error("Socket not bound");
        }
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &addr.sin_addr);
        
        ssize_t result = ::sendto(socket_, data.data(), data.size(), 0,
                                 reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        
        if (result == -1) {
            throw std::runtime_error("Failed to send UDP packet");
        }
    }
    
    std::vector<uint8_t> receive() override {
        if (socket_ == -1) {
            throw std::runtime_error("Socket not bound");
        }
        
        // Peek message size
        int size = 0;
        ioctl(socket_, FIONREAD, &size);
        
        if (size <= 0) {
            return {};
        }
        
        std::vector<uint8_t> buffer(size);
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);
        ssize_t result = recvfrom(socket_, buffer.data(), buffer.size(), MSG_PEEK,
                                reinterpret_cast<sockaddr*>(&from), &from_len);
        
        if (result == -1) {
            throw std::runtime_error("Failed to peek packet size");
        }
        
        buffer.resize(result);
        return buffer;
    }
    
    std::pair<std::string, uint16_t> receive_from() override {
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);
        char buffer[65536];
        ssize_t result = recvfrom(socket_, buffer, sizeof(buffer), 0,
                                reinterpret_cast<sockaddr*>(&from), &from_len);
        
        if (result == -1) {
            throw std::runtime_error("Failed to receive packet");
        }
        
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, ip_str, sizeof(ip_str));
        return {std::string(ip_str), ntohs(from.sin_port)};
    }
    
    bool is_connected() const override {
        return socket_ != -1 && is_bound_;
    }
    
    void close() override {
        if (socket_ != -1) {
            ::close(socket_);
            socket_ = -1;
        }
        is_bound_ = false;
    }
    
    bool has_data() const override {
        int size = 0;
        ioctl(socket_, FIONREAD, &size);
        return size > 0;
    }

private:
    int socket_;
    bool is_bound_ = false;
};

#endif // Platform-specific implementations

// Generic fallback socket
class GenericUdpSocket : public UdpSocket {
public:
    void bind(const std::string& /*address*/, uint16_t /*port*/) override {
        throw std::runtime_error("Generic UDP socket not implemented");
    }
    
    void send(const std::string& /*address*/, uint16_t /*port*/, 
             const std::vector<uint8_t>& /*data*/) override {
        throw std::runtime_error("Generic UDP socket not implemented");
    }
    
    std::vector<uint8_t> receive() override {
        throw std::runtime_error("Generic UDP socket not implemented");
    }
    
    std::pair<std::string, uint16_t> receive_from() override {
        throw std::runtime_error("Generic UDP socket not implemented");
    }
    
    bool is_connected() const override {
        return false;
    }
    
    void close() override {}
    
    bool has_data() const override {
        return false;
    }
};

} // namespace hyperion::network::udp