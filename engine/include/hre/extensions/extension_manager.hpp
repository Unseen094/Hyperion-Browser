#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>

namespace hre::extensions {

// Manifest V3 Structure
struct ExtensionManifest {
    std::wstring name;
    std::wstring version;
    std::wstring description;
    std::vector<std::wstring> permissions;
    std::map<std::wstring, std::wstring> background;
    std::map<std::wstring, std::wstring> action; // browser_action / page_action
    std::wstring manifest_version = L"3";
    
    bool is_valid() const { return !name.empty() && !version.empty(); }
};

// Extension Instance
class Extension {
public:
    Extension(const std::filesystem::path& path);
    
    bool load();
    void unload();
    
    const ExtensionManifest& manifest() const { return m_manifest; }
    bool is_loaded() const { return m_loaded; }
    
    // Runtime info
    std::wstring id() const { return m_id; }
    
private:
    std::filesystem::path m_path;
    ExtensionManifest m_manifest;
    bool m_loaded = false;
    std::wstring m_id;
    
    bool parse_manifest();
};

// Global Extension Manager
class ExtensionManager {
public:
    static ExtensionManager& instance();
    
    // Load/Unload
    bool load_extension(const std::wstring& path);
    void unload_extension(const std::wstring& id);
    
    // Access
    std::vector<std::shared_ptr<Extension>> get_extensions() const;
    std::shared_ptr<Extension> get_extension(const std::wstring& id);
    
    // Chrome API stubs (to be implemented in script layer)
    void register_chrome_api();

private:
    ExtensionManager() = default;
    std::map<std::wstring, std::shared_ptr<Extension>> m_extensions;
};

} // namespace hre::extensions
