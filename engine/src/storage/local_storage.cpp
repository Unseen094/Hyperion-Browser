#include "hre/storage/local_storage.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cwchar>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>

namespace hre::storage {

std::unordered_map<std::wstring, std::wstring> local_storage::s_store;
std::mutex local_storage::s_mutex;

// Path for persistent storage
static const std::wstring k_storage_path = L"C:/Users/rehan/Documents/projects/Hyperion/data/local_storage.json";

// Simple JSON parser for our use case
static std::wstring json_escape(const std::wstring& str) {
    std::wstring escaped;
    for (wchar_t c : str) {
        switch (c) {
            case L'\'': escaped += L"\\'"; break;
            case L'\"': escaped += L"\\\""; break;
            case L'\n': escaped += L"\\n"; break;
            case L'\r': escaped += L"\\r"; break;
            case L'\t': escaped += L"\\t"; break;
            case L'\b': escaped += L"\\b"; break;
            case L'\f': escaped += L"\\f"; break;
            case L'\\': escaped += L"\\\\"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

static std::wstring json_unescape(const std::wstring& str) {
    std::wstring unescaped;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == L'\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case L'\"': unescaped += L'\"'; ++i; break;
                case L'\'': unescaped += L'\''; ++i; break;
                case L'n': unescaped += L'\n'; ++i; break;
                case L'r': unescaped += L'\r'; ++i; break;
                case L't': unescaped += L'\t'; ++i; break;
                case L'b': unescaped += L'\b'; ++i; break;
                case L'f': unescaped += L'\f'; ++i; break;
                case L'\\': unescaped += L'\\'; ++i; break;
                default: unescaped += str[i]; break;
            }
        } else {
            unescaped += str[i];
        }
    }
    return unescaped;
}

static void load_from_file() {
    std::wifstream file(k_storage_path);
    if (!file.is_open()) return;
    
    std::wstring content((std::istreambuf_iterator<wchar_t>(file)),
                        std::istreambuf_iterator<wchar_t>());
    
    // Remove whitespace
    content.erase(std::remove_if(content.begin(), content.end(), ::iswspace), content.end());
    
    if (content.empty() || content[0] != L'{' || content.back() != L'}') return;
    
    content = content.substr(1, content.length() - 2);
    
    size_t pos = 0;
    while (pos < content.length()) {
        // Find key
        size_t quote_start = content.find(L'\"', pos);
        if (quote_start == std::wstring::npos) break;
        size_t quote_end = content.find(L'\"', quote_start + 1);
        if (quote_end == std::wstring::npos) break;
        
        std::wstring key = json_unescape(content.substr(quote_start + 1, quote_end - quote_start - 1));
        
        // Skip to value
        pos = quote_end + 1;
        if (pos >= content.length() || content[pos] != L':') break;
        ++pos;
        
        // Find value
        while (pos < content.length() && content[pos] == L' ') ++pos;
        
        if (pos >= content.length()) break;
        
        size_t value_start = pos;
        int brace_count = 0;
        bool in_string = false;
        
        while (pos < content.length()) {
            if (content[pos] == L'\"') {
                in_string = !in_string;
            } else if (!in_string) {
                if (content[pos] == L'{') ++brace_count;
                else if (content[pos] == L'}') --brace_count;
                else if (content[pos] == L',' && brace_count == 0) break;
            }
            ++pos;
        }
        
        std::wstring value_str = content.substr(value_start, pos - value_start);
        if (value_str.length() > 0 && value_str[0] == L'\"') {
            value_str = value_str.substr(1, value_str.length() - 2);
            value_str = json_unescape(value_str);
        }
        
        local_storage::s_store[key] = value_str;
        
        // Skip comma
        if (pos < content.length() && content[pos] == L',') ++pos;
    }
}

static void save_to_file() {
    std::wstring dir = L"C:/Users/rehan/Documents/projects/Hyperion/data";
    std::wstring file = k_storage_path;
    
    // Create directory if needed
    std::wstring cmd = L"mkdir \"" + dir + L"\"";
    _wsystem(cmd.c_str());
    
    std::wstring json = L"{\n";
    bool first = true;
    for (const auto& [key, value] : local_storage::s_store) {
        if (!first) json += L",\n";
        json += L"  \"" + json_escape(key) + L"\": \"" + json_escape(value) + L"\"";
        first = false;
    }
    json += L"\n}";
    
    std::wofstream file_out(file);
    if (file_out.is_open()) {
        file_out << json;
        file_out.close();
    }
}

// Initialize on first use
static bool loaded = false;
static std::once_flag init_flag;

static void initialize() {
    std::call_once(init_flag, []() {
        load_from_file();
        loaded = true;
    });
}

void local_storage::set_item(const std::wstring& key, const std::wstring& value) {
    initialize();
    std::lock_guard<std::mutex> lock(s_mutex);
    s_store[key] = value;
    save_to_file();
}

std::wstring local_storage::get_item(const std::wstring& key) {
    initialize();
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_store.find(key);
    return it != s_store.end() ? it->second : L"";
}

void local_storage::remove_item(const std::wstring& key) {
    initialize();
    std::lock_guard<std::mutex> lock(s_mutex);
    s_store.erase(key);
    save_to_file();
}

void local_storage::clear() {
    initialize();
    std::lock_guard<std::mutex> lock(s_mutex);
    s_store.clear();
    save_to_file();
}

} // namespace hre::storage
