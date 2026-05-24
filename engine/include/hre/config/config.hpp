#pragma once

#include <string>
#include <map>
#include <variant>
#include <filesystem>

namespace hre::config {

using ConfigValue = std::variant<int, double, std::wstring, bool>;

class Config {
public:
    static Config& instance();
    
    void load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path);
    
    // Getters
    int get_int(const std::wstring& key, int default_val = 0) const;
    double get_double(const std::wstring& key, double default_val = 0.0) const;
    std::wstring get_string(const std::wstring& key, const std::wstring& default_val = L"") const;
    bool get_bool(const std::wstring& key, bool default_val = false) const;
    
    // Setters
    void set(const std::wstring& key, const ConfigValue& value);

private:
    Config() = default;
    std::map<std::wstring, ConfigValue> m_data;
};

} // namespace hre::config
