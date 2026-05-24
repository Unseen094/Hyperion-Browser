#include <hjs/wasm/wasm_table.hpp>
#include <algorithm>
#include <stdexcept>

namespace hjs::wasm {

WasmTable::WasmTable(WasmTableElementType elem_type, uint32_t initial_size, uint32_t max_size)
    : m_elem_type(elem_type)
    , m_size(initial_size)
    , m_max_size(max_size)
    , m_entries(initial_size)
{
}

WasmTableEntry WasmTable::get(uint32_t index) const {
    if (index >= m_size) {
        return {WasmTableElementType::ExternRef, Value(), false};
    }
    return m_entries[index];
}

bool WasmTable::set(uint32_t index, const WasmTableEntry& entry) {
    if (index >= m_size) return false;
    if (entry.type != m_elem_type) return false;
    m_entries[index] = entry;
    return true;
}

uint32_t WasmTable::grow(uint32_t delta, const WasmTableEntry& fill_value) {
    uint32_t old_size = m_size;
    if (delta == 0) return old_size;

    uint64_t new_size = (uint64_t)m_size + delta;
    if (new_size > m_max_size) return (uint32_t)-1;

    m_entries.resize((size_t)new_size, fill_value);
    m_size = (uint32_t)new_size;
    return old_size;
}

WasmTableRegistry& WasmTableRegistry::instance() {
    static WasmTableRegistry registry;
    return registry;
}

uint32_t WasmTableRegistry::create_table(WasmTableElementType elem_type, uint32_t initial, uint32_t max_size) {
    auto table = std::make_unique<WasmTable>(elem_type, initial, max_size);
    uint32_t index = (uint32_t)m_tables.size();
    m_tables.push_back(std::move(table));
    return index;
}

WasmTable* WasmTableRegistry::get_table(uint32_t index) {
    if (index >= m_tables.size()) return nullptr;
    return m_tables[index].get();
}

bool WasmTableRegistry::destroy_table(uint32_t index) {
    if (index >= m_tables.size()) return false;
    m_tables.erase(m_tables.begin() + index);
    return true;
}

} // namespace hjs::wasm
