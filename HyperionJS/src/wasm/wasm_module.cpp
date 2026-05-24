#include <hjs/wasm/wasm_module.hpp>
#include <cstring>
#include <sstream>

namespace hjs::wasm {

static const uint8_t MAGIC[] = {0x00, 0x61, 0x73, 0x6D};
static const uint8_t VERSION[] = {0x01, 0x00, 0x00, 0x00};

bool WasmModule::parse(const std::vector<uint8_t>& binary) {
    size_t offset = 0;

    if (!read_magic(binary, offset)) {
        m_error = "Invalid WASM magic number";
        return false;
    }

    while (offset < binary.size()) {
        if (!read_section(binary, offset)) {
            if (!m_error.empty()) break;
        }
    }

    m_valid = m_error.empty();
    return m_valid;
}

bool WasmModule::read_magic(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset + 8 > data.size()) {
        m_error = "File too small";
        return false;
    }
    if (memcmp(data.data() + offset, MAGIC, 4) != 0) {
        m_error = "Bad magic number";
        return false;
    }
    offset += 4;
    if (memcmp(data.data() + offset, VERSION, 4) != 0) {
        m_error = "Unsupported WASM version";
        return false;
    }
    offset += 4;
    return true;
}

bool WasmModule::read_section(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size()) return false;

    uint8_t section_id = data[offset++];
    uint32_t section_size = read_uleb128(data, offset);
    size_t section_end = offset + section_size;

    if (section_end > data.size()) {
        m_error = "Section exceeds file size";
        return false;
    }

    switch (static_cast<WasmSection>(section_id)) {
        case WasmSection::Type: read_type_section(data, offset); break;
        case WasmSection::Import: read_import_section(data, offset); break;
        case WasmSection::Function: read_function_section(data, offset); break;
        case WasmSection::Export: read_export_section(data, offset); break;
        case WasmSection::Code: read_code_section(data, offset); break;
        default: break;
    }

    offset = section_end;
    return true;
}

uint32_t WasmModule::read_uleb128(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t result = 0;
    int shift = 0;
    while (offset < data.size()) {
        uint8_t byte = data[offset++];
        result |= (byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}

int32_t WasmModule::read_leb128(const std::vector<uint8_t>& data, size_t& offset) {
    int32_t result = 0;
    int shift = 0;
    while (offset < data.size()) {
        uint8_t byte = data[offset++];
        result |= (byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if (shift < 32 && (byte & 0x40)) result |= -(1 << shift);
            break;
        }
    }
    return result;
}

bool WasmModule::read_type_section(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = read_uleb128(data, offset);
    for (uint32_t i = 0; i < count; i++) {
        if (offset >= data.size()) return false;
        if (data[offset++] != 0x60) {
            m_error = "Expected functype";
            return false;
        }
        WasmFunctionType ft;
        uint32_t param_count = read_uleb128(data, offset);
        for (uint32_t j = 0; j < param_count; j++) {
            ft.params.push_back(data[offset++]);
        }
        uint32_t result_count = read_uleb128(data, offset);
        for (uint32_t j = 0; j < result_count; j++) {
            ft.results.push_back(data[offset++]);
        }
        m_types.push_back(ft);
    }
    return true;
}

bool WasmModule::read_import_section(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = read_uleb128(data, offset);
    for (uint32_t i = 0; i < count; i++) {
        WasmImport imp;
        uint32_t mod_len = read_uleb128(data, offset);
        std::string mod_str((const char*)data.data() + offset, mod_len);
        offset += mod_len;
        imp.module = std::wstring(mod_str.begin(), mod_str.end());

        uint32_t field_len = read_uleb128(data, offset);
        std::string field_str((const char*)data.data() + offset, field_len);
        offset += field_len;
        imp.field = std::wstring(field_str.begin(), field_str.end());

        imp.kind = data[offset++];
        if (imp.kind == 0) { // function import
            imp.type_index = read_uleb128(data, offset);
        }
        m_imports.push_back(imp);
    }
    return true;
}

bool WasmModule::read_function_section(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = read_uleb128(data, offset);
    for (uint32_t i = 0; i < count; i++) {
        m_function_indices.push_back(read_uleb128(data, offset));
    }
    return true;
}

bool WasmModule::read_export_section(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = read_uleb128(data, offset);
    for (uint32_t i = 0; i < count; i++) {
        WasmExport exp;
        uint32_t name_len = read_uleb128(data, offset);
        std::string name_str((const char*)data.data() + offset, name_len);
        offset += name_len;
        exp.name = std::wstring(name_str.begin(), name_str.end());
        exp.kind = data[offset++];
        exp.index = read_uleb128(data, offset);
        m_exports.push_back(exp);
    }
    return true;
}

bool WasmModule::read_code_section(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = read_uleb128(data, offset);
    for (uint32_t i = 0; i < count; i++) {
        WasmCodeEntry entry;
        entry.body_size = read_uleb128(data, offset);
        size_t code_start = offset;

        uint32_t local_count = read_uleb128(data, offset);
        for (uint32_t j = 0; j < local_count; j++) {
            uint32_t count_read = read_uleb128(data, offset);
            uint8_t type = data[offset++];
            for (uint32_t k = 0; k < count_read; k++) {
                entry.locals.push_back(type);
            }
        }

        // Copy remaining bytecode
        size_t consumed = offset - code_start;
        if (entry.body_size >= consumed) {
            entry.bytecode.insert(entry.bytecode.end(),
                data.begin() + offset,
                data.begin() + offset + (entry.body_size - consumed));
            offset += (entry.body_size - consumed);
        }

        m_code.push_back(entry);
    }
    return true;
}

// ---- WasmRuntime ---------------------------------------------------------

WasmRuntime::WasmRuntime(std::shared_ptr<WasmModule> mod) : m_module(mod) {}

Value WasmRuntime::call_export(const std::wstring& name, const std::vector<Value>&) {
    for (auto& exp : m_module->exports()) {
        if (exp.name == name && exp.kind == 0) {
            return Value((double)exp.index);
        }
    }
    return Value();
}

} // namespace hjs::wasm
