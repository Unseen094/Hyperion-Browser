#include "hre/config/config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace hre::config {

Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::load(const std::filesystem::path& path) {
    std::wifstream file(path);
    if (!file.is_open()) {
        // Create default config
        m_data[L"home_page"] = L"https://www.google.com";
        m_data[L"enable_extensions"] = true;
        m_data[L"enable_devtools"] = true;
        save(path);
        return;
    }
    
    std::wstringstream buffer;
    buffer << file.rdbuf();
    std::wstring content = buffer.str();
    
    // Simple key=value parser
    std::wistringstream iss(content);
    std::wstring line;
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == L'#') continue; // Skip comments
        
        size_t eq = line.find(L'=');
        if (eq != std::wstring::npos) {
            std::wstring key = line.substr(0, eq);
            std::wstring value = line.substr(eq + 1);
            
            // Trim whitespace
            while (!key.empty() && (key.back() == L' ' || key.back() == L'\t')) key.pop_back();
            while (!value.empty() && (value.front() == L' ' || value.front() == L'\t')) value = value.substr(1);
            
            // Try to parse type
            if (value == L"true") m_data[key] = true;
            else if (value == L"false") m_data[key] = false;
            else {
                try {
                    m_data[key] = std::stod(value);
                } catch (...) {
                    m_data[key] = value;
                }
            }
        }
    }
}

void Config::save(const std::filesystem::path& path) {
    std::wofstream file(path);
    if (!file.is_open()) return;
    
    file << L"# Hyperion Browser Configuration\n";
    file << L"# Generated automatically\n\n";
    
    for (const auto& [key, val] : m_data) {
        file << key << L"=";
        if (std::holds_alternative<std::wstring>(val)) {
            file << std::get<std::wstring>(val);
        } else if (std::holds_alternative<bool>(val)) {
            file << (std::get<bool>(val) ? L"true" : L"false");
        } else if (std::holds_alternative<double>(val)) {
            file << std::get<double>(val);
        } else if (std::holds_alternative<int>(val)) {
            file << std::get<int>(val);
        }
        file << L"\n";
    }
}

int Config::get_int(const std::wstring& key, int default_val) const {
    auto it = m_data.find(key);
    if (it == m_data.end()) return default_val;
    
    if (std::holds_alternative<int>(it->second)) return std::get<int>(it->second);
    if (std::holds_alternative<double>(it->second)) return (int)std::get<double>(it->second);
    return default_val;
}

double Config::get_double(const std::wstring& key, double default_val) const {
    auto it = m_data.find(key);
    if (it == m_data.end()) return default_val;
    
    if (std::holds_alternative<double>(it->second)) return std::get<double>(it->second);
    if (std::holds_alternative<int>(it->second)) return (double)std::get<int>(it->second);
    return default_val;
}

std::wstring Config::get_string(const std::wstring& key, const std::wstring& default_val) const {
    auto it = m_data.find(key);
    if (it == m_data.end()) return default_val;
    
    if (std::holds_alternative<std::wstring>(it->second)) return std::get<std::wstring>(it->second);
    return default_val;
}

bool Config::get_bool(const std::wstring& key, bool default_val) const {
    auto it = m_data.find(key);
    if (it == m_data.end()) return default_val;
    
    if (std::holds_alternative<bool>(it->second)) return std::get<bool>(it->second);
    return default_val;
}

void Config::set(const std::wstring& key, const ConfigValue& value) {
    m_data[key] = value;
}

} // namespace hre::config
