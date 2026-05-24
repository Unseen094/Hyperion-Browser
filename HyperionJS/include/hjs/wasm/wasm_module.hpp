#pragma once

#include <hjs/core/value.hpp>
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

namespace hjs::wasm {

enum class WasmSection : uint8_t {
    Custom = 0,
    Type = 1,
    Import = 2,
    Function = 3,
    Table = 4,
    Memory = 5,
    Global = 6,
    Export = 7,
    Start = 8,
    Element = 9,
    Code = 10,
    Data = 11
};

struct WasmFunctionType {
    std::vector<uint8_t> params;
    std::vector<uint8_t> results;
};

struct WasmImport {
    std::wstring module;
    std::wstring field;
    uint8_t kind; // 0=function, 1=table, 2=memory, 3=global
    int type_index = -1;
};

struct WasmExport {
    std::wstring name;
    uint8_t kind;
    int index;
};

struct WasmCodeEntry {
    int body_size;
    std::vector<uint8_t> locals;
    std::vector<uint8_t> bytecode;
};

class WasmModule {
public:
    WasmModule() = default;

    bool parse(const std::vector<uint8_t>& binary);
    const std::vector<WasmFunctionType>& function_types() const { return m_types; }
    const std::vector<WasmImport>& imports() const { return m_imports; }
    const std::vector<WasmExport>& exports() const { return m_exports; }
    const std::vector<WasmCodeEntry>& code_entries() const { return m_code; }
    bool valid() const { return m_valid; }
    const std::string& error() const { return m_error; }

private:
    bool read_magic(const std::vector<uint8_t>& data, size_t& offset);
    bool read_section(const std::vector<uint8_t>& data, size_t& offset);
    bool read_type_section(const std::vector<uint8_t>& data, size_t& offset);
    bool read_import_section(const std::vector<uint8_t>& data, size_t& offset);
    bool read_function_section(const std::vector<uint8_t>& data, size_t& offset);
    bool read_export_section(const std::vector<uint8_t>& data, size_t& offset);
    bool read_code_section(const std::vector<uint8_t>& data, size_t& offset);

    uint32_t read_uleb128(const std::vector<uint8_t>& data, size_t& offset);
    int32_t read_leb128(const std::vector<uint8_t>& data, size_t& offset);

    std::vector<WasmFunctionType> m_types;
    std::vector<WasmImport> m_imports;
    std::vector<WasmExport> m_exports;
    std::vector<WasmCodeEntry> m_code;
    std::vector<int> m_function_indices;
    bool m_valid = false;
    std::string m_error;
};

class WasmRuntime {
public:
    WasmRuntime(std::shared_ptr<WasmModule> mod);
    Value call_export(const std::wstring& name, const std::vector<Value>& args);

private:
    std::shared_ptr<WasmModule> m_module;
};

} // namespace hjs::wasm
