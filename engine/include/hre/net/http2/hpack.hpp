#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <utility>

namespace hre::net::http2 {

struct header_entry {
    std::string name;
    std::string value;
    bool sensitive;

    header_entry() : sensitive(false) {}
    header_entry(std::string n, std::string v, bool s = false)
        : name(std::move(n)), value(std::move(v)), sensitive(s) {}
};

class hpack_encoder {
public:
    hpack_encoder();
    std::vector<uint8_t> encode(const std::vector<header_entry>& headers);
    void set_max_table_size(uint32_t size);

private:
    std::vector<header_entry> m_dynamic_table;
    uint32_t m_max_table_size;
    uint32_t m_current_table_size;

    void append_indexed(uint32_t index, std::vector<uint8_t>& out);
    void append_literal(const std::string& name, const std::string& value,
                        bool name_indexed, std::vector<uint8_t>& out);
    void append_huffman(const std::string& input, std::vector<uint8_t>& out);
    void evict_if_needed();
    uint32_t find_entry(const std::string& name, const std::string& value) const;
    uint32_t find_name(const std::string& name) const;
};

class hpack_decoder {
public:
    hpack_decoder();
    std::vector<header_entry> decode(const std::vector<uint8_t>& data, size_t& offset);
    void set_max_table_size(uint32_t size);

private:
    std::vector<header_entry> m_dynamic_table;
    uint32_t m_max_table_size;
    uint32_t m_current_table_size;

    header_entry decode_entry(const std::vector<uint8_t>& data, size_t& offset);
    std::string decode_huffman(const std::vector<uint8_t>& data, size_t& offset);
    uint32_t decode_integer(const std::vector<uint8_t>& data, size_t& offset, uint8_t prefix);
    std::string decode_string(const std::vector<uint8_t>& data, size_t& offset);
    void evict_if_needed();
    void add_dynamic(const std::string& name, const std::string& value);
};

const std::vector<header_entry>& static_table();
uint32_t static_table_size();

} // namespace hre::net::http2
