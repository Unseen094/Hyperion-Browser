#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>
#include <bit>

namespace hyperion::network::quic {

// QUIC version (draft or official)
enum class QuicVersion : uint32_t {
    V1 = 0x00000001,
    DRAFT_29 = 0xFF00001D,
    DRAFT_30 = 0xFF00001E,
};

// QUIC packet types
// Long header packets
constexpr uint8_t PACKET_TYPE_INITIAL = 0x00;
constexpr uint8_t PACKET_TYPE_0RTT = 0x01;
constexpr uint8_t PACKET_TYPE_HANDSHAKE = 0x02;
constexpr uint8_t PACKET_TYPE_RETRY = 0x03;

// Short header packets
constexpr uint8_t PACKET_TYPE_1RTT = 0x04;

// QUIC packet header (simplified format for Phase 13)
struct QuicLongHeader {
    uint8_t type;           // Packet type
    QuicVersion version;    // QUIC version
    std::array<uint8_t, 16> connection_id; // Connection ID (CID)
    uint32_t packet_number;  // Packet number (counters per packet type/space)
};

// QUIC frame types
constexpr uint8_t FRAME_TYPE_PADDING = 0x00;
constexpr uint8_t FRAME_TYPE_PING = 0x01;
constexpr uint8_t FRAME_TYPE_ACK = 0x02;
constexpr uint8_t FRAME_TYPE_ACK_ECN = 0x03;
constexpr uint8_t FRAME_TYPE_RESET_STREAM = 0x04;
constexpr uint8_t FRAME_TYPE_STOP_SENDING = 0x05;
constexpr uint8_t FRAME_TYPE_CRYPTO = 0x06;
constexpr uint8_t FRAME_TYPE_NEW_TOKEN = 0x07;
constexpr uint8_t FRAME_TYPE_STREAM = 0x08;

// QUIC ACK frame
struct AckFrame {
    uint64_t largest_acknowledged;
    uint64_t ack_delay;
    std::vector<uint64_t> ack_ranges;
};

// QUIC STREAM frame
struct StreamFrame {
    uint32_t stream_id;
    uint64_t offset;
    bool has_offset;
    bool has_length;
    uint16_t length;
    std::vector<uint8_t> data;
    bool is_fin;
};

// QUIC packet parser
class QuicPacket {
public:
    static std::vector<uint8_t> create_initial_packet(
        const std::array<uint8_t, 16>& connection_id,
        uint32_t packet_number,
        const std::vector<uint8_t>& payload,
        QuicVersion version = QuicVersion::V1
    ) {
        // QUIC Initial packet format (simplified for Phase 13)
        // 0                   1                   2                   3
        // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        // +-+-+-+-+-+-+-+-+
        // |1|           Packet Number (7..16..32)          |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // ~                     Connection ID (64..160)            ~
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // ~                  [Version]                     ~
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |                    Token Length (i)
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // ~                     Token (if any)                   ~
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |                     Length (i)
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // ~                     Frame(s) (variable length)         ~
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        std::vector<uint8_t> packet;
        
        // Minimal Initial packet: 0xC3 (11000011) + 32-bit packet number
        // Using 7-bit packet number encoding for simplicity
        packet.push_back(0xC3 | ((packet_number >> 24) & 0x03));
        
        // Connection ID (simplified to 8 bytes for Phase 13)
        packet.push_back(0x08); // 8-byte CID length
        packet.insert(packet.end(), connection_id.begin(), connection_id.begin() + 8);
        
        // Version (4 bytes)
        uint32_t version_net = static_cast<uint32_t>(version);
        packet.push_back((version_net >> 24) & 0xFF);
        packet.push_back((version_net >> 16) & 0xFF);
        packet.push_back((version_net >> 8) & 0xFF);
        packet.push_back(version_net & 0xFF);
        
        // Token length (0 for no token)
        packet.push_back(0x00);
        
        // Length (2 bytes)
        uint16_t payload_len = static_cast<uint16_t>(payload.size());
        packet.push_back((payload_len >> 8) & 0xFF);
        packet.push_back(payload_len & 0xFF);
        
        // Payload
        packet.insert(packet.end(), payload.begin(), payload.end());
        
        return packet;
    }
    
    static std::vector<uint8_t> parse_packet(const std::vector<uint8_t>& packet) {
        if (packet.empty()) {
            throw std::runtime_error("Empty QUIC packet");
        }
        
        // Minimal parsing - just extract payload for Phase 13
        // Real implementation would parse headers, connection ID, version, etc.
        
        std::vector<uint8_t> payload;
        if (packet.size() > 2) {
            // Last 2 bytes are payload length
            uint16_t payload_len = (static_cast<uint16_t>(packet[packet.size() - 2]) << 8) | 
                                 static_cast<uint16_t>(packet[packet.size() - 1]);
            
            if (packet.size() >= 2 + payload_len) {
                payload.assign(packet.end() - payload_len - 2, packet.end() - 2);
            }
        }
        
        return payload;
    }
    
    static std::vector<uint8_t> create_ack_frame(uint64_t largest_ack, uint64_t ack_delay) {
        std::vector<uint8_t> frame;
        frame.push_back(FRAME_TYPE_ACK);
        
        // Add largest acknowledged
        append_varint(frame, largest_ack);
        
        // Add ACK delay
        append_varint(frame, ack_delay);
        
        // Add ACK ranges count (0 for now)
        frame.push_back(0x00); // No ranges
        
        return frame;
    }
    
    static AckFrame parse_ack_frame(const std::vector<uint8_t>& data) {
        if (data.empty()) {
            throw std::runtime_error("Empty ACK frame");
        }
        
        size_t idx = 0;
        AckFrame ack;
        
        // Parse largest acknowledged
        ack.largest_acknowledged = parse_varint(data, idx);
        
        // Parse ACK delay
        ack.ack_delay = parse_varint(data, idx);
        
        // Parse number of ack ranges
        uint8_t range_count = data[idx++];
        for (uint8_t i = 0; i < range_count; ++i) {
            // Simple implementation - real QUIC has gap and length
            uint64_t gap = parse_varint(data, idx);
            uint64_t length = parse_varint(data, idx);
            ack.ack_ranges.push_back(gap);
        }
        
        return ack;
    }
    
    static std::vector<uint8_t> create_stream_frame(const StreamFrame& stream) {
        std::vector<uint8_t> frame;
        frame.push_back(FRAME_TYPE_STREAM | (stream.is_fin ? 0x01 : 0x00));
        
        // Stream ID
        append_varint(frame, stream.stream_id);
        
        // Offset (if present)
        if (stream.has_offset) {
            frame.push_back(0x10); // Stream offset flag
            append_varint(frame, stream.offset);
        }
        
        // Length (if present)
        if (stream.has_length) {
            frame.push_back(0x02); // Length present flag
            uint16_t len = stream.length;
            frame.push_back((len >> 8) & 0xFF);
            frame.push_back(len & 0xFF);
        }
        
        // Data
        frame.insert(frame.end(), stream.data.begin(), stream.data.end());
        
        return frame;
    }
    
    static StreamFrame parse_stream_frame(const std::vector<uint8_t>& data) {
        if (data.empty() || (data[0] & 0xF8) != FRAME_TYPE_STREAM) {
            throw std::runtime_error("Not a STREAM frame");
        }
        
        size_t idx = 1;
        StreamFrame frame;
        frame.is_fin = (data[0] & 0x01) != 0;
        frame.has_offset = (data[0] & 0x10) != 0;
        frame.has_length = (data[0] & 0x02) != 0;
        
        // Parse stream ID
        frame.stream_id = static_cast<uint32_t>(parse_varint(data, idx));
        
        // Parse offset
        if (frame.has_offset) {
            uint64_t offset_code = parse_varint(data, idx);
            frame.offset = offset_code;
        }
        
        // Parse length
        if (frame.has_length && idx + 2 <= data.size()) {
            frame.length = (static_cast<uint16_t>(data[idx]) << 8) | 
                         static_cast<uint16_t>(data[idx + 1]);
            idx += 2;
        }
        
        // Parse data
        if (idx < data.size()) {
            frame.data.assign(data.begin() + idx, data.end());
        }
        
        return frame;
    }
    
    static std::vector<uint8_t> create_settings_frame(const std::vector<std::pair<uint64_t, uint64_t>>& settings) {
        std::vector<uint8_t> frame;
        frame.push_back(FRAME_TYPE_CRYPTO);
        
        // Settings negotiation parameters (HTTP/3)
        frame.push_back(0x00); // 6-bit varint encoding start
        frame.push_back(0x04); // SETTINGS
        
        // QuicTransportParametersType
        append_varint(frame, 0x0000000000000001); // Enable HTTP/3
        append_varint(frame, 0x0000000000000000);
        
        return frame;
    }
    
    static std::vector<uint8_t> create_push_promise(uint32_t stream_id, const std::vector<uint8_t>& headers) {
        std::vector<uint8_t> frame;
        frame.push_back(0x05); // PUSH_PROMISE frame type base
        append_varint(frame, stream_id | 0x80000000); // Stream ID with reserved bit
        
        // Add headers (placeholder for QPACK/H29)
        frame.insert(frame.end(), headers.begin(), headers.end());
        return frame;
    }
    
    static uint8_t packet_type_from_first_byte(uint8_t byte) {
        if ((byte & 0x80) != 0) {
            // Long header
            if ((byte & 0x40) == 0x40) {
                return PACKET_TYPE_INITIAL;
            }
            if ((byte & 0x30) == 0x20) {
                return PACKET_TYPE_0RTT;
            }
            return PACKET_TYPE_HANDSHAKE;
        }
        // Short header
        return PACKET_TYPE_1RTT;
    }
    
    static bool is_long_header_packet(uint8_t type) {
        return type <= PACKET_TYPE_RETRY;
    }
};

// HTTP/3 connection identifiers manager (Phase 13)
class Http3ConnectionIds {
public:
    void add_connection_id(const std::array<uint8_t, 16>& cid) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ids_.push_back(cid);
        if (active_ids_.size() > MAX_ACTIVE_IDS) {
            active_ids_.erase(active_ids_.begin());
        }
    }
    
    bool has_connection_id(const std::array<uint8_t, 16>& cid) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& id : active_ids_) {
            if (id == cid) {
                return true;
            }
        }
        return false;
    }

private:
    static constexpr size_t MAX_ACTIVE_IDS = 1000;
    std::vector<std::array<uint8_t, 16>> active_ids_;
    std::mutex mutex_;
};
    // Variable-length integer encoding (RFC 9000)
    static void append_varint(std::vector<uint8_t>& data, uint64_t value) {
        if (value < 64) {
            data.push_back(static_cast<uint8_t>(value));
        } else if (value < 16384) {
            data.push_back(static_cast<uint8_t>((value >> 8) | 0x40));
            data.push_back(static_cast<uint8_t>(value & 0xFF));
        } else if (value < 1073741824) {
            data.push_back(static_cast<uint8_t>((value >> 24) | 0x80));
            data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>(value & 0xFF));
        } else {
            data.push_back(0xC0 | static_cast<uint8_t>((value >> 56) & 0x03));
            data.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>(value & 0xFF));
        }
    }
    
    static uint64_t parse_varint(const std::vector<uint8_t>& data, size_t& idx) {
        if (idx >= data.size()) {
            throw std::runtime_error("Varint parsing error: out of bounds");
        }
        
        uint8_t first = data[idx++];
        if ((first & 0xC0) == 0x00) {
            // 6-bit encoding
            return static_cast<uint64_t>(first & 0x3F);
        } else if ((first & 0xC0) == 0x40) {
            // 14-bit encoding
            if (idx + 1 > data.size()) {
                throw std::runtime_error("Varint parsing error: incomplete 14-bit");
            }
            uint16_t val = (static_cast<uint16_t>(first & 0x3F) << 8) | 
                          static_cast<uint16_t>(data[idx++]);
            return static_cast<uint64_t>(val);
        } else if ((first & 0x80) == 0x80) {
            // 30-bit encoding
            if (idx + 3 > data.size()) {
                throw std::runtime_error("Varint parsing error: incomplete 30-bit");
            }
            uint32_t val = (static_cast<uint32_t>(first & 0x7F) << 24) |
                          (static_cast<uint32_t>(data[idx++]) << 16) |
                          (static_cast<uint32_t>(data[idx++]) << 8) |
                          static_cast<uint32_t>(data[idx++]);
            return static_cast<uint64_t>(val);
        } else {
            // 62-bit encoding
            if (idx + 7 > data.size()) {
                throw std::runtime_error("Varint parsing error: incomplete 62-bit");
            }
            uint64_t val = (static_cast<uint64_t>(first & 0x3F) << 56);
            for (int i = 0; i < 7; ++i) {
                val |= static_cast<uint64_t>(data[idx++]) << (56 - (i + 1) * 8);
            }
            return val;
        }
    }
};

} // namespace hyperion::network::quic