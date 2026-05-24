#pragma once

#include <hjs/core/value.hpp>
#include <vector>
#include <cstdint>
#include <memory>

namespace hjs::wasm {

enum class WasmTableElementType : uint8_t {
    FuncRef = 0x70,
    ExternRef = 0x6F,
    AnyRef = 0x6E
};

struct WasmTableEntry {
    WasmTableElementType type;
    Value value;
    bool valid = false;
};

class WasmTable {
public:
    WasmTable(WasmTableElementType elem_type, uint32_t initial_size, uint32_t max_size = UINT32_MAX);

    WasmTableEntry get(uint32_t index) const;
    bool set(uint32_t index, const WasmTableEntry& entry);
    uint32_t size() const { return m_size; }
    uint32_t grow(uint32_t delta, const WasmTableEntry& fill_value);

    WasmTableElementType element_type() const { return m_elem_type; }
    uint32_t max_size() const { return m_max_size; }

    bool is_valid_index(uint32_t index) const { return index < m_size; }

private:
    WasmTableElementType m_elem_type;
    uint32_t m_size;
    uint32_t m_max_size;
    std::vector<WasmTableEntry> m_entries;
};

class WasmTableRegistry {
public:
    static WasmTableRegistry& instance();

    uint32_t create_table(WasmTableElementType elem_type, uint32_t initial, uint32_t max_size = UINT32_MAX);
    WasmTable* get_table(uint32_t index);
    bool destroy_table(uint32_t index);
    uint32_t table_count() const { return (uint32_t)m_tables.size(); }

private:
    WasmTableRegistry() = default;
    std::vector<std::unique_ptr<WasmTable>> m_tables;
};

} // namespace hjs::wasm
