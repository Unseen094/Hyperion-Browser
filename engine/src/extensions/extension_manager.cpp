#include "hre/extensions/extension_manager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// Simple JSON parser helper (minimal for manifest.json)
static std::wstring get_json_string(const std::wstring& json, const std::wstring& key) {
    std::wstring search = L"\"" + key + L"\"";
    size_t pos = json.find(search);
    if (pos == std::wstring::npos) return L"";
    
    pos = json.find(L":", pos);
    if (pos == std::wstring::npos) return L"";
    
    pos = json.find(L"\"", pos);
    if (pos == std::wstring::npos) return L"";
    
    size_t end = json.find(L"\"", pos + 1);
    if (end == std::wstring::npos) return L"";
    
    return json.substr(pos + 1, end - pos - 1);
}

namespace hre::extensions {

Extension::Extension(const std::filesystem::path& path) 
    : m_path(path), m_loaded(false) {
    // Generate a simple ID based on path
    m_id = path.filename().wstring();
}

bool Extension::load() {
    if (m_loaded) return true;
    
    if (parse_manifest() && m_manifest.is_valid()) {
        m_loaded = true;
        std::wcout << L"Extension loaded: " << m_manifest.name << std::endl;
        return true;
    }
    
    std::wcerr << L"Failed to load extension: " << m_path.wstring() << std::endl;
    return false;
}

void Extension::unload() {
    if (m_loaded) {
        m_loaded = false;
        std::wcout << L"Extension unloaded: " << m_manifest.name << std::endl;
    }
}

bool Extension::parse_manifest() {
    std::filesystem::path manifest_path = m_path / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        return false;
    }
    
    std::wifstream file(manifest_path);
    if (!file.is_open()) {
        return false;
    }
    
    std::wstringstream buffer;
    buffer << file.rdbuf();
    std::wstring json = buffer.str();
    
    m_manifest.name = get_json_string(json, L"name");
    m_manifest.version = get_json_string(json, L"version");
    m_manifest.description = get_json_string(json, L"description");
    m_manifest.manifest_version = get_json_string(json, L"manifest_version");
    
    return m_manifest.is_valid();
}

ExtensionManager& ExtensionManager::instance() {
    static ExtensionManager inst;
    return inst;
}

bool ExtensionManager::load_extension(const std::wstring& path) {
    std::filesystem::path fs_path(path);
    if (!std::filesystem::exists(fs_path)) {
        return false;
    }
    
    auto ext = std::make_shared<Extension>(fs_path);
    if (ext->load()) {
        m_extensions[ext->id()] = ext;
        return true;
    }
    
    return false;
}

void ExtensionManager::unload_extension(const std::wstring& id) {
    auto it = m_extensions.find(id);
    if (it != m_extensions.end()) {
        it->second->unload();
        m_extensions.erase(it);
    }
}

std::vector<std::shared_ptr<Extension>> ExtensionManager::get_extensions() const {
    std::vector<std::shared_ptr<Extension>> result;
    for (const auto& pair : m_extensions) {
        result.push_back(pair.second);
    }
    return result;
}

std::shared_ptr<Extension> ExtensionManager::get_extension(const std::wstring& id) {
    auto it = m_extensions.find(id);
    if (it != m_extensions.end()) {
        return it->second;
    }
    return nullptr;
}

void ExtensionManager::register_chrome_api() {
    // This would register the chrome.* APIs in the JS VM
    // Implementation in script layer
}

} // namespace hre::extensions
