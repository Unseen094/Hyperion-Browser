#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>

namespace hre::net::http2 {

enum class frame_type : uint8_t {
    DATA = 0x0,
    HEADERS = 0x1,
    PRIORITY = 0x2,
    RST_STREAM = 0x3,
    SETTINGS = 0x4,
    PUSH_PROMISE = 0x5,
    PING = 0x6,
    GOAWAY = 0x7,
    WINDOW_UPDATE = 0x8,
    CONTINUATION = 0x9
};

enum class settings_id : uint16_t {
    HEADER_TABLE_SIZE = 0x1,
    ENABLE_PUSH = 0x2,
    MAX_CONCURRENT_STREAMS = 0x3,
    INITIAL_WINDOW_SIZE = 0x4,
    MAX_FRAME_SIZE = 0x5,
    MAX_HEADER_LIST_SIZE = 0x6
};

enum class error_code : uint32_t {
    H2_NO_ERROR = 0x0,
    PROTOCOL_ERROR = 0x1,
    INTERNAL_ERROR = 0x2,
    FLOW_CONTROL_ERROR = 0x3,
    SETTINGS_TIMEOUT = 0x4,
    STREAM_CLOSED = 0x5,
    FRAME_SIZE_ERROR = 0x6,
    REFUSED_STREAM = 0x7,
    CANCEL = 0x8,
    COMPRESSION_ERROR = 0x9,
    CONNECT_ERROR = 0xA,
    ENHANCE_YOUR_CALM = 0xB,
    INADEQUATE_SECURITY = 0xC,
    HTTP_1_1_REQUIRED = 0xD
};

enum class frame_flag : uint8_t {
    END_STREAM = 0x1,
    END_HEADERS = 0x4,
    PADDED = 0x8,
    PRIORITY = 0x20
};

struct frame_header {
    uint32_t length : 24;
    frame_type type;
    uint8_t flags;
    uint32_t stream_id : 31;
    bool reserved : 1;

    frame_header()
        : length(0), type(frame_type::DATA), flags(0), stream_id(0), reserved(false) {}
};

struct frame {
    frame_header header;
    std::vector<uint8_t> payload;

    std::vector<uint8_t> serialize() const;
    static frame deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

struct settings_frame {
    std::map<settings_id, uint32_t> settings;

    std::vector<uint8_t> serialize() const;
    static settings_frame deserialize(const std::vector<uint8_t>& payload);
};

struct goaway_frame {
    uint32_t last_stream_id;
    error_code code;
    std::string debug_data;

    std::vector<uint8_t> serialize() const;
    static goaway_frame deserialize(const std::vector<uint8_t>& payload);
};

struct window_update_frame {
    uint32_t window_size_increment;

    std::vector<uint8_t> serialize() const;
    static window_update_frame deserialize(const std::vector<uint8_t>& payload);
};

struct priority_frame {
    uint32_t stream_id;
    bool exclusive;
    uint32_t parent_stream_id;
    uint8_t weight;

    std::vector<uint8_t> serialize() const;
    static priority_frame deserialize(const std::vector<uint8_t>& payload);
};

} // namespace hre::net::http2
