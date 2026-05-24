#include <hjs/wasm/wasm_multi_value.hpp>
#include <cstring>
#include <algorithm>

namespace hjs::wasm {

WasmMultiValueInterpreter::WasmMultiValueInterpreter() = default;

bool WasmMultiValueInterpreter::push_results(const std::vector<Value>& values) {
    WasmMultiValueBlock block;
    block.values = values;
    block.valid = true;
    m_call_stack.push_back(block);
    for (const auto& v : values) {
        m_value_stack.push_back(v);
    }
    return true;
}

std::vector<Value> WasmMultiValueInterpreter::pop_results(size_t count) {
    std::vector<Value> results;
    if (m_call_stack.empty()) return results;

    WasmMultiValueBlock& block = m_call_stack.back();
    size_t to_pop = std::min(count, block.values.size());
    for (size_t i = 0; i < to_pop && !m_value_stack.empty(); i++) {
        results.push_back(m_value_stack.back());
        m_value_stack.pop_back();
    }
    std::reverse(results.begin(), results.end());

    if (block.values.size() <= count) {
        m_call_stack.pop_back();
    } else {
        block.values.erase(block.values.begin(), block.values.begin() + count);
    }

    return results;
}

void WasmMultiValueInterpreter::clear_stack() {
    m_value_stack.clear();
    m_call_stack.clear();
}

Value WasmMultiValueInterpreter::get_result(size_t index) const {
    if (m_call_stack.empty()) return Value();
    const auto& block = m_call_stack.back();
    if (index < block.values.size()) return block.values[index];
    return Value();
}

void WasmMultiValueInterpreter::set_result(size_t index, const Value& val) {
    if (m_call_stack.empty()) return;
    auto& block = m_call_stack.back();
    if (index < block.values.size()) {
        block.values[index] = val;
        size_t stack_idx = m_value_stack.size() - block.values.size() + index;
        if (stack_idx < m_value_stack.size()) {
            m_value_stack[stack_idx] = val;
        }
    }
}

bool wasm_supports_multi_value(const std::vector<uint8_t>& binary) {
    if (binary.size() < 8) return false;
    const uint8_t magic[] = {0x00, 0x61, 0x73, 0x6D};
    const uint8_t version[] = {0x01, 0x00, 0x00, 0x00};
    if (memcmp(binary.data(), magic, 4) != 0) return false;
    if (memcmp(binary.data() + 4, version, 4) != 0) return false;

    size_t offset = 8;
    while (offset < binary.size()) {
        if (offset + 1 > binary.size()) break;
        uint8_t section_id = binary[offset++];
        if (section_id != 1) {
            uint32_t section_size = 0;
            int shift = 0;
            while (offset < binary.size()) {
                uint8_t b = binary[offset++];
                section_size |= (b & 0x7F) << shift;
                shift += 7;
                if ((b & 0x80) == 0) break;
            }
            offset += section_size;
            continue;
        }

        uint32_t section_size = 0;
        int shift = 0;
        while (offset < binary.size()) {
            uint8_t b = binary[offset++];
            section_size |= (b & 0x7F) << shift;
            shift += 7;
            if ((b & 0x80) == 0) break;
        }

        size_t section_end = offset + section_size;
        if (section_end > binary.size()) return false;

        uint32_t type_count = 0;
        shift = 0;
        while (offset < section_end) {
            uint8_t b = binary[offset++];
            type_count |= (b & 0x7F) << shift;
            shift += 7;
            if ((b & 0x80) == 0) break;
        }

        for (uint32_t i = 0; i < type_count && offset < section_end; i++) {
            if (offset >= section_end) break;
            if (binary[offset++] != 0x60) continue;

            uint32_t param_count = 0;
            shift = 0;
            while (offset < section_end) {
                uint8_t b = binary[offset++];
                param_count |= (b & 0x7F) << shift;
                shift += 7;
                if ((b & 0x80) == 0) break;
            }
            offset += param_count;

            uint32_t result_count = 0;
            shift = 0;
            while (offset < section_end) {
                uint8_t b = binary[offset++];
                result_count |= (b & 0x7F) << shift;
                shift += 7;
                if ((b & 0x80) == 0) break;
            }

            if (result_count > 1) return true;
            offset += result_count;
        }
        break;
    }
    return false;
}

std::vector<WasmFunctionType> extract_multi_value_types(const WasmModule& mod) {
    std::vector<WasmFunctionType> mv_types;
    for (const auto& ft : mod.function_types()) {
        if (ft.results.size() > 1) {
            mv_types.push_back(ft);
        }
    }
    return mv_types;
}

} // namespace hjs::wasm
