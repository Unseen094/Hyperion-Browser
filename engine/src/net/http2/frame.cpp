#include "hre/net/http2/frame.hpp"
#include <cstring>
#include <algorithm>

namespace hre::net::http2 {

static void write_uint16(std::vector<uint8_t>& out, uint16_t val) {
    out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(val & 0xFF));
}

static uint16_t read_uint16(const std::vector<uint8_t>& data, size_t& offset) {
    uint16_t val = (static_cast<uint16_t>(data[offset]) << 8) | static_cast<uint16_t>(data[offset + 1]);
    offset += 2;
    return val;
}

static void write_uint24(std::vector<uint8_t>& out, uint32_t val) {
    out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(val & 0xFF));
}

static void write_uint32(std::vector<uint8_t>& out, uint32_t val) {
    out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(val & 0xFF));
}

static uint32_t read_uint24(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t val = (static_cast<uint32_t>(data[offset]) << 16)
                 | (static_cast<uint32_t>(data[offset + 1]) << 8)
                 | static_cast<uint32_t>(data[offset + 2]);
    offset += 3;
    return val;
}

static uint32_t read_uint32(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t val = (static_cast<uint32_t>(data[offset]) << 24)
                 | (static_cast<uint32_t>(data[offset + 1]) << 16)
                 | (static_cast<uint32_t>(data[offset + 2]) << 8)
                 | static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    return val;
}

std::vector<uint8_t> frame::serialize() const {
    std::vector<uint8_t> out;
    write_uint24(out, header.length);
    out.push_back(static_cast<uint8_t>(header.type));
    out.push_back(header.flags);
    uint32_t stream_id_field = header.stream_id & 0x7FFFFFFF;
    if (header.reserved) stream_id_field |= 0x80000000;
    write_uint32(out, stream_id_field);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

frame frame::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    frame f;
    f.header.length = read_uint24(data, offset);
    f.header.type = static_cast<frame_type>(data[offset++]);
    f.header.flags = data[offset++];
    uint32_t stream_id_field = read_uint32(data, offset);
    f.header.reserved = (stream_id_field & 0x80000000) != 0;
    f.header.stream_id = stream_id_field & 0x7FFFFFFF;
    if (f.header.length > 0 && offset + f.header.length <= data.size()) {
        f.payload.assign(data.begin() + offset, data.begin() + offset + f.header.length);
        offset += f.header.length;
    }
    return f;
}

std::vector<uint8_t> settings_frame::serialize() const {
    std::vector<uint8_t> out;
    for (const auto& [id, val] : settings) {
        write_uint16(out, static_cast<uint16_t>(id));
        write_uint32(out, val);
    }
    return out;
}

settings_frame settings_frame::deserialize(const std::vector<uint8_t>& payload) {
    settings_frame sf;
    size_t offset = 0;
    while (offset + 6 <= payload.size()) {
        auto id = static_cast<settings_id>(read_uint16(payload, offset));
        uint32_t val = read_uint32(payload, offset);
        sf.settings[id] = val;
    }
    return sf;
}

std::vector<uint8_t> goaway_frame::serialize() const {
    std::vector<uint8_t> out;
    write_uint32(out, last_stream_id);
    write_uint32(out, static_cast<uint32_t>(code));
    out.insert(out.end(), debug_data.begin(), debug_data.end());
    return out;
}

goaway_frame goaway_frame::deserialize(const std::vector<uint8_t>& payload) {
    goaway_frame gf;
    size_t offset = 0;
    gf.last_stream_id = read_uint32(payload, offset);
    gf.code = static_cast<error_code>(read_uint32(payload, offset));
    if (offset < payload.size()) {
        gf.debug_data.assign(payload.begin() + offset, payload.end());
    }
    return gf;
}

std::vector<uint8_t> window_update_frame::serialize() const {
    std::vector<uint8_t> out;
    uint32_t field = window_size_increment & 0x7FFFFFFF;
    write_uint32(out, field);
    return out;
}

window_update_frame window_update_frame::deserialize(const std::vector<uint8_t>& payload) {
    window_update_frame wf;
    size_t offset = 0;
    wf.window_size_increment = read_uint32(payload, offset) & 0x7FFFFFFF;
    return wf;
}

std::vector<uint8_t> priority_frame::serialize() const {
    std::vector<uint8_t> out;
    uint32_t field = stream_id & 0x7FFFFFFF;
    if (exclusive) field |= 0x80000000;
    write_uint32(out, field);
    out.push_back(weight);
    return out;
}

priority_frame priority_frame::deserialize(const std::vector<uint8_t>& payload) {
    priority_frame pf;
    size_t offset = 0;
    uint32_t field = read_uint32(payload, offset);
    pf.exclusive = (field & 0x80000000) != 0;
    pf.stream_id = field & 0x7FFFFFFF;
    pf.weight = payload[offset++];
    return pf;
}

} // namespace hre::net::http2
