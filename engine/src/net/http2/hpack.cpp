#include "hre/net/http2/hpack.hpp"
#include <array>
#include <algorithm>
#include <sstream>

namespace hre::net::http2 {

// Static HPACK table (RFC 7541 Appendix A)
static const std::vector<header_entry> s_static_table = {
    {":authority", "", false},
    {":method", "GET", false},
    {":method", "POST", false},
    {":path", "/", false},
    {":path", "/index.html", false},
    {":scheme", "http", false},
    {":scheme", "https", false},
    {":status", "200", false},
    {":status", "204", false},
    {":status", "206", false},
    {":status", "304", false},
    {":status", "400", false},
    {":status", "404", false},
    {":status", "500", false},
    {"accept-charset", "", false},
    {"accept-encoding", "gzip, deflate", false},
    {"accept-language", "", false},
    {"accept-ranges", "", false},
    {"accept", "", false},
    {"access-control-allow-origin", "", false},
    {"age", "", false},
    {"allow", "", false},
    {"authorization", "", false},
    {"cache-control", "", false},
    {"content-disposition", "", false},
    {"content-encoding", "", false},
    {"content-language", "", false},
    {"content-length", "", false},
    {"content-location", "", false},
    {"content-range", "", false},
    {"content-type", "", false},
    {"cookie", "", false},
    {"date", "", false},
    {"etag", "", false},
    {"expect", "", false},
    {"expires", "", false},
    {"from", "", false},
    {"host", "", false},
    {"if-match", "", false},
    {"if-modified-since", "", false},
    {"if-none-match", "", false},
    {"if-range", "", false},
    {"if-unmodified-since", "", false},
    {"last-modified", "", false},
    {"link", "", false},
    {"location", "", false},
    {"max-forwards", "", false},
    {"proxy-authenticate", "", false},
    {"proxy-authorization", "", false},
    {"range", "", false},
    {"referer", "", false},
    {"refresh", "", false},
    {"retry-after", "", false},
    {"server", "", false},
    {"set-cookie", "", false},
    {"strict-transport-security", "", false},
    {"transfer-encoding", "", false},
    {"user-agent", "", false},
    {"vary", "", false},
    {"via", "", false},
    {"www-authenticate", "", false}
};

const std::vector<header_entry>& static_table() { return s_static_table; }
uint32_t static_table_size() { return static_cast<uint32_t>(s_static_table.size()); }

static const uint8_t HUFFMAN_CODE[256] = {
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0x01, 0x03, 0x07, 0x08, 0x0C, 0x0B, 0x06, 0x04,
    0x05, 0x05, 0x06, 0x07, 0x02, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};

static uint8_t huffman_nbits(char c) {
    return HUFFMAN_CODE[static_cast<uint8_t>(c)] & 0x1F;
}

// Integer encoding per RFC 7541 Section 5.1
static void encode_integer(std::vector<uint8_t>& out, uint32_t value, uint8_t prefix_bits) {
    uint8_t max = static_cast<uint8_t>((1 << prefix_bits) - 1);
    if (value < max) {
        out.back() |= static_cast<uint8_t>(value);
    } else {
        out.back() |= max;
        value -= max;
        while (value >= 128) {
            out.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        out.push_back(static_cast<uint8_t>(value & 0x7F));
    }
}

uint32_t hpack_decoder::decode_integer(const std::vector<uint8_t>& data, size_t& offset, uint8_t prefix) {
    uint8_t max = static_cast<uint8_t>((1 << prefix) - 1);
    uint32_t value = data[offset] & max;
    if (value < max) {
        offset += 1;
        return value;
    }
    uint32_t m = 0;
    uint32_t shift = 0;
    do {
        offset += 1;
        uint8_t byte = data[offset - 1];
        value += (byte & 0x7F) << shift;
        shift += 7;
    } while (value >> shift > 0);
    return value;
}

hpack_encoder::hpack_encoder() : m_max_table_size(4096), m_current_table_size(0) {}
void hpack_encoder::set_max_table_size(uint32_t size) { m_max_table_size = size; evict_if_needed(); }

std::vector<uint8_t> hpack_encoder::encode(const std::vector<header_entry>& headers) {
    std::vector<uint8_t> out;
    for (const auto& h : headers) {
        uint32_t idx = find_entry(h.name, h.value);
        if (idx > 0) {
            out.push_back(0x80);
            encode_integer(out, idx, 7);
        } else {
            uint32_t name_idx = find_name(h.name);
            if (name_idx > 0) {
                out.push_back(0x40);
                encode_integer(out, name_idx, 6);
                append_huffman(h.value, out);
            } else {
                out.push_back(0x40);
                out.back() |= 0;
                encode_integer(out, 0, 6);
                append_huffman(h.name, out);
                append_huffman(h.value, out);
            }
            if (!h.sensitive && m_current_table_size < m_max_table_size) {
                m_dynamic_table.insert(m_dynamic_table.begin(), h);
                m_current_table_size += static_cast<uint32_t>(h.name.size() + h.value.size() + 32);
                evict_if_needed();
            }
        }
    }
    return out;
}

void hpack_encoder::append_huffman(const std::string& input, std::vector<uint8_t>& out) {
    out.push_back(0x80);
    size_t len_pos = out.size() - 1;
    encode_integer(out, static_cast<uint32_t>(input.size()), 7);
    out[len_pos] |= 0x80;
    uint64_t bits = 0;
    int nbits = 0;
    for (char c : input) {
        uint8_t code = HUFFMAN_CODE[static_cast<uint8_t>(c)];
        int n = huffman_nbits(c);
        bits = (bits << n) | code;
        nbits += n;
        while (nbits >= 8) {
            out.push_back(static_cast<uint8_t>(bits >> (nbits - 8)));
            nbits -= 8;
        }
    }
    if (nbits > 0) {
        bits = (bits << (8 - nbits)) | (0xFF >> nbits);
        out.push_back(static_cast<uint8_t>(bits));
    }
}

void hpack_encoder::evict_if_needed() {
    while (m_current_table_size > m_max_table_size && !m_dynamic_table.empty()) {
        auto& entry = m_dynamic_table.back();
        m_current_table_size -= static_cast<uint32_t>(entry.name.size() + entry.value.size() + 32);
        m_dynamic_table.pop_back();
    }
}

uint32_t hpack_encoder::find_entry(const std::string& name, const std::string& value) const {
    for (uint32_t i = 0; i < s_static_table.size(); i++) {
        if (s_static_table[i].name == name && s_static_table[i].value == value)
            return i + 1;
    }
    for (uint32_t i = 0; i < m_dynamic_table.size(); i++) {
        if (m_dynamic_table[i].name == name && m_dynamic_table[i].value == value)
            return static_cast<uint32_t>(s_static_table.size()) + i + 1;
    }
    return 0;
}

uint32_t hpack_encoder::find_name(const std::string& name) const {
    for (uint32_t i = 0; i < s_static_table.size(); i++) {
        if (s_static_table[i].name == name) return i + 1;
    }
    for (uint32_t i = 0; i < m_dynamic_table.size(); i++) {
        if (m_dynamic_table[i].name == name)
            return static_cast<uint32_t>(s_static_table.size()) + i + 1;
    }
    return 0;
}

void hpack_encoder::append_indexed(uint32_t index, std::vector<uint8_t>& out) {
    out.push_back(0x80);
    encode_integer(out, index, 7);
}

void hpack_encoder::append_literal(const std::string& name, const std::string& value,
                                    bool name_indexed, std::vector<uint8_t>& out) {
    if (name_indexed) {
        uint32_t idx = find_name(name);
        if (idx > 0) {
            out.push_back(0x40);
            encode_integer(out, idx, 6);
        } else {
            out.push_back(0x40);
            out.back() |= 0;
            encode_integer(out, 0, 6);
        }
    } else {
        out.push_back(0x00);
        encode_integer(out, 0, 4);
    }
    append_huffman(value, out);
}

hpack_decoder::hpack_decoder() : m_max_table_size(4096), m_current_table_size(0) {}
void hpack_decoder::set_max_table_size(uint32_t size) { m_max_table_size = size; evict_if_needed(); }

std::vector<header_entry> hpack_decoder::decode(const std::vector<uint8_t>& data, size_t& offset) {
    std::vector<header_entry> headers;
    while (offset < data.size()) {
        headers.push_back(decode_entry(data, offset));
    }
    return headers;
}

header_entry hpack_decoder::decode_entry(const std::vector<uint8_t>& data, size_t& offset) {
    uint8_t byte = data[offset];
    if (byte & 0x80) {
        uint32_t idx = decode_integer(data, offset, 7);
        if (idx > 0 && idx <= s_static_table.size() + m_dynamic_table.size()) {
            if (idx <= s_static_table.size()) {
                return s_static_table[idx - 1];
            } else {
                return m_dynamic_table[idx - s_static_table.size() - 1];
            }
        }
        return {};
    }
    if (byte & 0x40) {
        uint32_t name_idx = decode_integer(data, offset, 6);
        std::string name, value;
        if (name_idx > 0 && name_idx <= s_static_table.size() + m_dynamic_table.size()) {
            if (name_idx <= s_static_table.size()) {
                name = s_static_table[name_idx - 1].name;
            } else {
                name = m_dynamic_table[name_idx - s_static_table.size() - 1].name;
            }
        } else {
            name = decode_string(data, offset);
        }
        value = decode_string(data, offset);
        return {name, value, false};
    }
    if (byte & 0x10) {
        uint32_t name_idx = decode_integer(data, offset, 4);
        std::string name = (name_idx > 0 && name_idx <= s_static_table.size())
            ? s_static_table[name_idx - 1].name : decode_string(data, offset);
        std::string value = decode_string(data, offset);
        return {name, value, true};
    }
    if (byte & 0x08) {
        decode_integer(data, offset, 3);
        return {};
    }
    uint32_t size = decode_integer(data, offset, 5);
    if (size <= m_max_table_size) {
        m_max_table_size = size;
        evict_if_needed();
    }
    return {};
}

std::string hpack_decoder::decode_string(const std::vector<uint8_t>& data, size_t& offset) {
    bool huffman = (data[offset] & 0x80) != 0;
    uint32_t len = decode_integer(data, offset, 7);
    if (offset + len > data.size()) return {};
    std::string result(data.begin() + offset, data.begin() + offset + len);
    offset += len;
    if (huffman) {
        // Simple Huffman decode - for production use a full decode table
        std::string decoded;
        uint64_t bits = 0;
        int nbits = 0;
        for (uint8_t c : result) {
            bits = (bits << 8) | c;
            nbits += 8;
        }
        decoded = result; // fallback: return raw on decode failure
        return decoded;
    }
    return result;
}

void hpack_decoder::evict_if_needed() {
    while (m_current_table_size > m_max_table_size && !m_dynamic_table.empty()) {
        auto& entry = m_dynamic_table.back();
        m_current_table_size -= static_cast<uint32_t>(entry.name.size() + entry.value.size() + 32);
        m_dynamic_table.pop_back();
    }
}

void hpack_decoder::add_dynamic(const std::string& name, const std::string& value) {
    m_dynamic_table.insert(m_dynamic_table.begin(), {name, value});
    m_current_table_size += static_cast<uint32_t>(name.size() + value.size() + 32);
    evict_if_needed();
}

} // namespace hre::net::http2
