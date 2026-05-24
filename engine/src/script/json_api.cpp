#include "hre/script/json_api.hpp"
#include <hjs/runtime/object.hpp>
#include <sstream>

namespace hre::script {

// Very basic JSON.stringify implementation (strings, numbers, booleans, null, simple objects)
hjs::Value native_json_stringify(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1) return hjs::Value(L"undefined");

    const auto& val = args[0];
    if (val.is_string()) {
        // Escape string
        std::wstring s = val.as_string();
        std::wstring result = L"\"";
        for (wchar_t c : s) {
            if (c == L'"') result += L"\\\"";
            else if (c == L'\\') result += L"\\\\";
            else if (c == L'\n') result += L"\\n";
            else if (c == L'\r') result += L"\\r";
            else if (c == L'\t') result += L"\\t";
            else result += c;
        }
        result += L"\"";
        return hjs::Value(result);
    } else if (val.is_number()) {
        return hjs::Value(val.to_string());
    } else if (val.is_boolean()) {
        return hjs::Value(val.as_boolean() ? L"true" : L"false");
    } else if (val.is_undefined() || val.is_null()) {
        return hjs::Value(L"null");
    } else if (val.is_object()) {
        // Simplified object stringification: { "key": value, ... }
        auto obj = val.as_object();
        std::wstring result = L"{";
        bool first = true;
        // Note: This is a very naive implementation; real engines iterate properties
        // For now, return "{}" for objects as a placeholder
        result += L"}";
        return hjs::Value(result);
    }

    return hjs::Value(L"null");
}

// Very basic JSON.parse implementation
hjs::Value native_json_parse(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) {
        return hjs::Value();
    }

    std::wstring json = args[0].as_string();
    // Trim whitespace
    size_t start = json.find_first_not_of(L" \t\n\r");
    if (start == std::wstring::npos) return hjs::Value();
    size_t end = json.find_last_not_of(L" \t\n\r");
    json = json.substr(start, end - start + 1);

    if (json == L"null") return hjs::Value();
    if (json == L"true") return hjs::Value(true);
    if (json == L"false") return hjs::Value(false);

    if (json[0] == L'"' && json.back() == L'"') {
        // String
        std::wstring content = json.substr(1, json.size() - 2);
        // Unescape
        std::wstring result;
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == L'\\' && i + 1 < content.size()) {
                wchar_t next = content[++i];
                if (next == L'n') result += L'\n';
                else if (next == L'r') result += L'\r';
                else if (next == L't') result += L'\t';
                else result += next;
            } else {
                result += content[i];
            }
        }
        return hjs::Value(result);
    }

    if (json[0] == L'-' || (json[0] >= L'0' && json[0] <= L'9')) {
        // Number
        try {
            return hjs::Value(std::stod(json));
        } catch (...) {
            return hjs::Value();
        }
    }

    // Object/Array not fully implemented in this basic version
    return hjs::Value();
}

} // namespace hre::script
