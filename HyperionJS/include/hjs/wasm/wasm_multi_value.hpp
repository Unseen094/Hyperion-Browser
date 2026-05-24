#pragma once

#include <hjs/wasm/wasm_module.hpp>
#include <vector>
#include <cstdint>

namespace hjs::wasm {

struct WasmMultiValueBlock {
    std::vector<Value> values;
    bool valid = false;
};

class WasmMultiValueInterpreter {
public:
    WasmMultiValueInterpreter();

    bool push_results(const std::vector<Value>& values);
    std::vector<Value> pop_results(size_t count);
    void clear_stack();

    size_t stack_depth() const { return m_value_stack.size(); }
    bool has_results() const { return !m_value_stack.empty(); }

    Value get_result(size_t index) const;
    void set_result(size_t index, const Value& val);

private:
    std::vector<Value> m_value_stack;
    std::vector<WasmMultiValueBlock> m_call_stack;
};

bool wasm_supports_multi_value(const std::vector<uint8_t>& binary);
std::vector<WasmFunctionType> extract_multi_value_types(const WasmModule& mod);

} // namespace hjs::wasm
