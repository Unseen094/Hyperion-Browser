#include "hre/storage/session_storage.hpp"

namespace hre::storage {

std::unordered_map<std::wstring, std::wstring> session_storage::s_store;
std::mutex session_storage::s_mutex;

void session_storage::set_item(const std::wstring& key, const std::wstring& value) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_store[key] = value;
}

std::wstring session_storage::get_item(const std::wstring& key) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_store.find(key);
    return it != s_store.end() ? it->second : L"";
}

void session_storage::remove_item(const std::wstring& key) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_store.erase(key);
}

void session_storage::clear() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_store.clear();
}

} // namespace hre::storage
