#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <stdexcept>
#include <bit>

namespace hyperion::network::http2 {

// HTTP/2 frame header (9 bytes)
struct alignas(8) FrameHeader {
    uint32_t length : 24;  // Frame payload length (24 bits)
    uint8_t type;           // Frame type
    uint8_t flags;          // Frame-specific flags
    uint32_t stream_id;     // Stream identifier (31 bits)
};

// HTTP/2 frame types
enum class FrameType : uint8_t {
    DATA = 0x0,
    HEADERS = 0x1,
    PRIORITY = 0x2,
    RST_STREAM = 0x3,
    SETTINGS = 0x4,
    PUSH_PROMISE = 0x5,
    PING = 0x6,
    GOAWAY = 0x7,
    WINDOW_UPDATE = 0x8,
    CONTINUATION = 0x9,
};

// Settings parameter IDs
enum class SettingsParam : uint16_t {
    HEADER_TABLE_SIZE = 0x1,
    ENABLE_PUSH = 0x2,
    MAX_CONCURRENT_STREAMS = 0x3,
    INITIAL_WINDOW_SIZE = 0x4,
    MAX_FRAME_SIZE = 0x5,
    MAX_HEADER_LIST_SIZE = 0x6,
};

// Priority weight (8 bits, 1-256)
struct PriorityWeight {
    uint8_t value; // 1-256 (stored as 0-255)
};

// HTTP/2 frame base class
class Frame {
public:
    virtual ~Frame() = default;
    virtual FrameType type() const = 0;
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual void parse(const std::vector<uint8_t>& data) = 0;
    
    const FrameHeader& header() const { return header_; }
    void set_header(const FrameHeader& header) { header_ = header; }
    
protected:
    FrameHeader header_;
};

// DATA frame
class DataFrame : public Frame {
public:
    FrameType type() const override { return FrameType::DATA; }
    
    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> result;
        result.resize(9 + payload_.size()); // Header + payload
        
        // Serialize header (network byte order)
        uint32_t length = static_cast<uint32_t>(payload_.size());
        result[0] = (length >> 16) & 0xFF;
        result[1] = (length >> 8) & 0xFF;
        result[2] = length & 0xFF;
        result[3] = static_cast<uint8_t>(type());
        result[4] = flags_;
        result[5] = (header_.stream_id >> 24) & 0xFF;
        result[6] = (header_.stream_id >> 16) & 0xFF;
        result[7] = (header_.stream_id >> 8) & 0xFF;
        result[8] = header_.stream_id & 0xFF;
        
        // Copy payload
        std::copy(payload_.begin(), payload_.end(), result.begin() + 9);
        return result;
    }
    
    void parse(const std::vector<uint8_t>& data) override {
        if (data.size() < 9) {
            throw std::runtime_error("Invalid DATA frame: insufficient size");
        }
        
        // Parse header
        header_.length = (static_cast<uint32_t>(data[0]) << 16) |
                         (static_cast<uint32_t>(data[1]) << 8) |
                         static_cast<uint32_t>(data[2]);
        header_.type = data[3];
        flags_ = data[4];
        header_.stream_id = (static_cast<uint32_t>(data[5]) << 24) |
                           (static_cast<uint32_t>(data[6]) << 16) |
                           (static_cast<uint32_t>(data[7]) << 8) |
                           static_cast<uint32_t>(data[8]);
        
        // Validate
        if (header_.type != static_cast<uint8_t>(FrameType::DATA)) {
            throw std::runtime_error("Invalid frame type for DATA frame");
        }
        
        // Parse payload
        payload_.resize(header_.length);
        std::copy(data.begin() + 9, data.begin() + 9 + header_.length, payload_.begin());
    }
    
    void set_payload(const std::vector<uint8_t>& payload) {
        payload_ = payload;
        header_.length = static_cast<uint32_t>(payload.size());
    }
    
    const std::vector<uint8_t>& payload() const { return payload_; }
    void set_flags(uint8_t flags) { flags_ = flags; }
    uint8_t flags() const { return flags_; }

private:
    std::vector<uint8_t> payload_;
    uint8_t flags_ = 0;
};

// HEADERS frame
class HeadersFrame : public Frame {
public:
    FrameType type() const override { return FrameType::HEADERS; }
    
    std::vector<uint8_t> serialize() const override {
        // Simplified for Phase 11 - actual HPACK compression not implemented
        std::vector<uint8_t> result;
        result.resize(9 + headers_.size());
        
        // Serialize header (network byte order)
        uint32_t length = static_cast<uint32_t>(headers_.size());
        result[0] = (length >> 16) & 0xFF;
        result[1] = (length >> 8) & 0xFF;
        result[2] = length & 0xFF;
        result[3] = static_cast<uint8_t>(type());
        result[4] = flags_;
        result[5] = (header_.stream_id >> 24) & 0xFF;
        result[6] = (header_.stream_id >> 16) & 0xFF;
        result[7] = (header_.stream_id >> 8) & 0xFF;
        result[8] = header_.stream_id & 0xFF;
        
        // Copy headers
        std::copy(headers_.begin(), headers_.end(), result.begin() + 9);
        return result;
    }
    
    void parse(const std::vector<uint8_t>& data) override {
        if (data.size() < 9) {
            throw std::runtime_error("Invalid HEADERS frame: insufficient size");
        }
        
        // Parse header
        header_.length = (static_cast<uint32_t>(data[0]) << 16) |
                         (static_cast<uint32_t>(data[1]) << 8) |
                         static_cast<uint32_t>(data[2]);
        header_.type = data[3];
        flags_ = data[4];
        header_.stream_id = (static_cast<uint32_t>(data[5]) << 24) |
                           (static_cast<uint32_t>(data[6]) << 16) |
                           (static_cast<uint32_t>(data[7]) << 8) |
                           static_cast<uint32_t>(data[8]);
        
        // Validate
        if (header_.type != static_cast<uint8_t>(FrameType::HEADERS)) {
            throw std::runtime_error("Invalid frame type for HEADERS frame");
        }
        
        // Parse headers
        headers_.resize(header_.length);
        std::copy(data.begin() + 9, data.begin() + 9 + header_.length, headers_.begin());
    }
    
    void set_headers(const std::vector<uint8_t>& headers) {
        headers_ = headers;
        header_.length = static_cast<uint32_t>(headers.size());
    }
    
    const std::vector<uint8_t>& headers() const { return headers_; }
    void set_flags(uint8_t flags) { flags_ = flags; }
    uint8_t flags() const { return flags_; }

private:
    std::vector<uint8_t> headers_;
    uint8_t flags_ = 0;
};

// SETTINGS frame
class SettingsFrame : public Frame {
public:
    FrameType type() const override { return FrameType::SETTINGS; }
    
    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> result;
        result.resize(9 + params_.size() * 6); // 6 bytes per param
        
        // Serialize header
        uint32_t length = static_cast<uint32_t>(params_.size() * 6);
        result[0] = (length >> 16) & 0xFF;
        result[1] = (length >> 8) & 0xFF;
        result[2] = length & 0xFF;
        result[3] = static_cast<uint8_t>(type());
        result[4] = flags_;
        result[5] = (header_.stream_id >> 24) & 0xFF;
        result[6] = (header_.stream_id >> 16) & 0xFF;
        result[7] = (header_.stream_id >> 8) & 0xFF;
        result[8] = header_.stream_id & 0xFF;
        
        // Serialize parameters
        size_t idx = 9;
        for (const auto& [param, value] : params_) {
            result[idx++] = static_cast<uint8_t>((static_cast<uint16_t>(param) >> 8) & 0xFF);
            result[idx++] = static_cast<uint8_t>(static_cast<uint16_t>(param) & 0xFF);
            result[idx++] = (value >> 24) & 0xFF;
            result[idx++] = (value >> 16) & 0xFF;
            result[idx++] = (value >> 8) & 0xFF;
            result[idx++] = value & 0xFF;
        }
        
        return result;
    }
    
    void parse(const std::vector<uint8_t>& data) override {
        if (data.size() < 9) {
            throw std::runtime_error("Invalid SETTINGS frame: insufficient size");
        }
        
        // Parse header
        header_.length = (static_cast<uint32_t>(data[0]) << 16) |
                         (static_cast<uint32_t>(data[1]) << 8) |
                         static_cast<uint32_t>(data[2]);
        header_.type = data[3];
        flags_ = data[4];
        header_.stream_id = (static_cast<uint32_t>(data[5]) << 24) |
                           (static_cast<uint32_t>(data[6]) << 16) |
                           (static_cast<uint32_t>(data[7]) << 8) |
                           static_cast<uint32_t>(data[8]);
        
        // Validate
        if (header_.type != static_cast<uint8_t>(FrameType::SETTINGS)) {
            throw std::runtime_error("Invalid frame type for SETTINGS frame");
        }
        
        // Parse parameters (each is 6 bytes)
        size_t param_count = header_.length / 6;
        params_.clear();
        for (size_t i = 0; i < param_count; ++i) {
            size_t offset = 9 + i * 6;
            if (offset + 5 >= data.size()) {
                throw std::runtime_error("Invalid SETTINGS frame: truncated parameters");
            }
            
            uint16_t param_id = (static_cast<uint16_t>(data[offset]) << 8) | 
                              static_cast<uint16_t>(data[offset + 1]);
            uint32_t value = (static_cast<uint32_t>(data[offset + 2]) << 24) |
                           (static_cast<uint32_t>(data[offset + 3]) << 16) |
                           (static_cast<uint32_t>(data[offset + 4]) << 8) |
                           static_cast<uint32_t>(data[offset + 5]);
            
            params_.emplace_back(static_cast<SettingsParam>(param_id), value);
        }
    }
    
    void add_parameter(SettingsParam param, uint32_t value) {
        params_.emplace_back(param, value);
        header_.length = static_cast<uint32_t>(params_.size() * 6);
    }
    
    const std::vector<std::pair<SettingsParam, uint32_t>>& parameters() const {
        return params_;
    }
    
    void set_flags(uint8_t flags) { flags_ = flags; }
    uint8_t flags() const { return flags_; }

private:
    std::vector<std::pair<SettingsParam, uint32_t>> params_;
    uint8_t flags_ = 0;
};

// Priority frame
class PriorityFrame : public Frame {
public:
    FrameType type() const override { return FrameType::PRIORITY; }
    
    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> result;
        result.resize(9 + 5); // Header + 5 bytes payload
        
        // Serialize header
        uint32_t length = 5;
        result[0] = (length >> 16) & 0xFF;
        result[1] = (length >> 8) & 0xFF;
        result[2] = length & 0xFF;
        result[3] = static_cast<uint8_t>(type());
        result[4] = 0; // No flags
        result[5] = (header_.stream_id >> 24) & 0xFF;
        result[6] = (header_.stream_id >> 16) & 0xFF;
        result[7] = (header_.stream_id >> 8) & 0xFF;
        result[8] = header_.stream_id & 0xFF;
        
        // Serialize payload: stream dependency (31 bits) + weight (8 bits)
        uint32_t exclusivity = (exclusive_ << 31);
        uint32_t dependency = stream_dependency_ | exclusivity;
        result[9] = (dependency >> 24) & 0x7F; // Mask high bit
        result[10] = (dependency >> 16) & 0xFF;
        result[11] = (dependency >> 8) & 0xFF;
        result[12] = dependency & 0xFF;
        result[13] = weight_;
        
        return result;
    }
    
    void parse(const std::vector<uint8_t>& data) override {
        if (data.size() < 14) {
            throw std::runtime_error("Invalid PRIORITY frame: insufficient size");
        }
        
        // Parse header
        header_.length = (static_cast<uint32_t>(data[0]) << 16) |
                        (static_cast<uint32_t>(data[1]) << 8) |
                        static_cast<uint32_t>(data[2]);
        header_.type = data[3];
        flags_ = data[4];
        header_.stream_id = (static_cast<uint32_t>(data[5]) << 24) |
                          (static_cast<uint32_t>(data[6]) << 16) |
                          (static_cast<uint32_t>(data[7]) << 8) |
                          static_cast<uint32_t>(data[8]);
        
        // Validate
        if (header_.type != static_cast<uint8_t>(FrameType::PRIORITY)) {
            throw std::runtime_error("Invalid frame type for PRIORITY frame");
        }
        
        // Parse payload
        uint32_t dependency = (static_cast<uint32_t>(data[9] & 0x7F) << 24) |
                            (static_cast<uint32_t>(data[10]) << 16) |
                            (static_cast<uint32_t>(data[11]) << 8) |
                            static_cast<uint32_t>(data[12]);
        exclusive_ = (data[9] & 0x80) != 0;
        stream_dependency_ = dependency;
        weight_ = data[13];
    }
    
    void set_priority(uint32_t stream_dependency, uint8_t weight, bool exclusive = false) {
        stream_dependency_ = stream_dependency;
        weight_ = weight;
        exclusive_ = exclusive;
        if (exclusive) {
            header_.length = 5;
        } else {
            header_.length = 5;
        }
    }
    
    uint32_t stream_dependency() const { return stream_dependency_; }
    uint8_t weight() const { return weight_; }
    bool exclusive() const { return exclusive_; }

private:
    uint32_t stream_dependency_ = 0;
    uint8_t weight_ = 16;
    bool exclusive_ = false;
    uint8_t flags_ = 0;
};

// RST_STREAM frame
class RstStreamFrame : public Frame {
public:
    FrameType type() const override { return FrameType::RST_STREAM; }
    
    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> result;
        result.resize(9 + 4); // Header + 4 bytes error code
        
        // Serialize header
        uint32_t length = 4;
        result[0] = (length >> 16) & 0xFF;
        result[1] = (length >> 8) & 0xFF;
        result[2] = length & 0xFF;
        result[3] = static_cast<uint8_t>(type());
        result[4] = flags_;
        result[5] = (header_.stream_id >> 24) & 0xFF;
        result[6] = (header_.stream_id >> 16) & 0xFF;
        result[7] = (header_.stream_id >> 8) & 0xFF;
        result[8] = header_.stream_id & 0xFF;
        
        // Serialize error code
        result[9] = (error_code_ >> 24) & 0xFF;
        result[10] = (error_code_ >> 16) & 0xFF;
        result[11] = (error_code_ >> 8) & 0xFF;
        result[12] = error_code_ & 0xFF;
        
        return result;
    }
    
    void parse(const std::vector<uint8_t>& data) override {
        if (data.size() < 13) {
            throw std::runtime_error("Invalid RST_STREAM frame: insufficient size");
        }
        
        // Parse header
        header_.length = (static_cast<uint32_t>(data[0]) << 16) |
                        (static_cast<uint32_t>(data[1]) << 8) |
                        static_cast<uint32_t>(data[2]);
        header_.type = data[3];
        flags_ = data[4];
        header_.stream_id = (static_cast<uint32_t>(data[5]) << 24) |
                          (static_cast<uint32_t>(data[6]) << 16) |
                          (static_cast<uint32_t>(data[7]) << 8) |
                          static_cast<uint32_t>(data[8]);
        
        // Validate
        if (header_.type != static_cast<uint8_t>(FrameType::RST_STREAM)) {
            throw std::runtime_error("Invalid frame type for RST_STREAM frame");
        }
        
        // Parse error code
        error_code_ = (static_cast<uint32_t>(data[9]) << 24) |
                     (static_cast<uint32_t>(data[10]) << 16) |
                     (static_cast<uint32_t>(data[11]) << 8) |
                     static_cast<uint32_t>(data[12]);
    }
    
    // HTTP/2 error codes
enum class ErrorCode : uint32_t {
        NO_ERROR = 0x0,
        PROTOCOL_ERROR = 0x1,
        INTERNAL_ERROR = 0x2,
        FLOW_CONTROL_ERROR = 0x3,
        SETTINGS_TIMEOUT = 0x4,
        STREAM_CLOSED = 0x5,
        FRAME_TOO_LARGE = 0x6,
        REFUSED_STREAM = 0x7,
        CANCEL = 0x8,
        COMPRESSION_ERROR = 0x9,
        CONNECT_ERROR = 0xA,
        ENHANCE_YOUR_CALM = 0xB,
        INADEQUATE_SECURITY = 0xC,
        HTTP_1_1_REQUIRED = 0xD,
    };
    
    void set_error_code(ErrorCode code) {
        error_code_ = static_cast<uint32_t>(code);
        header_.length = 4;
    }
    
    ErrorCode error_code() const { return static_cast<ErrorCode>(error_code_); }

private:
    uint32_t error_code_ = 0;
    uint8_t flags_ = 0;
};

// RST_STREAM frame
class RstStreamFrame : public Frame {
    // Placeholder for Phase 11 implementation
};

// Helper functions
inline uint16_t htons(uint16_t value) {
    return (value >> 8) | (value << 8);
}

inline uint32_t htonl(uint32_t value) {
    return ((value >> 24) & 0xFF) |
           ((value >> 8) & 0xFF00) |
           ((value << 8) & 0xFF0000) |
           ((value << 24) & 0xFF000000);
}
