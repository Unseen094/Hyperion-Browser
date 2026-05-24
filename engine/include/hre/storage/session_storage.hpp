#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace hre::storage {

class session_storage {
public:
    static void set_item(const std::wstring& key, const std::wstring& value);
    static std::wstring get_item(const std::wstring& key);
    static void remove_item(const std::wstring& key);
    static void clear();

private:
    static std::unordered_map<std::wstring, std::wstring> s_store;
    static std::mutex s_mutex;
};

} // namespace hre::storage
