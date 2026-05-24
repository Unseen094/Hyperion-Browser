#include "hre/script/fetch_api.hpp"
#include "hre/storage/local_storage.hpp"
#include "hre/storage/session_storage.hpp"

namespace hre::script {

hjs::Value native_fetch(hjs::Value receiver, int argc, hjs::Value* args) {
    return hjs::Value(true);
}

hjs::Value native_local_storage_get(hjs::Value, int argc, hjs::Value* args) {
    if (argc == 0) return hjs::Value(L"");
    std::wstring key = args[0].as_string();
    std::wstring value = hre::storage::local_storage::get_item(key);
    return hjs::Value(value);
}

hjs::Value native_local_storage_set(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 2) return hjs::Value(false);
    std::wstring key = args[0].as_string();
    std::wstring value = args[1].as_string();
    hre::storage::local_storage::set_item(key, value);
    return hjs::Value(true);
}

hjs::Value native_local_storage_remove(hjs::Value, int argc, hjs::Value* args) {
    if (argc == 0) return hjs::Value(false);
    std::wstring key = args[0].as_string();
    hre::storage::local_storage::remove_item(key);
    return hjs::Value(true);
}

hjs::Value native_local_storage_clear(hjs::Value, int argc, hjs::Value* args) {
    hre::storage::local_storage::clear();
    return hjs::Value(true);
}

hjs::Value native_session_storage_get(hjs::Value, int argc, hjs::Value* args) {
    if (argc == 0) return hjs::Value(L"");
    std::wstring key = args[0].as_string();
    std::wstring value = hre::storage::session_storage::get_item(key);
    return hjs::Value(value);
}

hjs::Value native_session_storage_set(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 2) return hjs::Value(false);
    std::wstring key = args[0].as_string();
    std::wstring value = args[1].as_string();
    hre::storage::session_storage::set_item(key, value);
    return hjs::Value(true);
}

hjs::Value native_session_storage_remove(hjs::Value, int argc, hjs::Value* args) {
    if (argc == 0) return hjs::Value(false);
    std::wstring key = args[0].as_string();
    hre::storage::session_storage::remove_item(key);
    return hjs::Value(true);
}

hjs::Value native_session_storage_clear(hjs::Value, int argc, hjs::Value* args) {
    hre::storage::session_storage::clear();
    return hjs::Value(true);
}

} // namespace hre::script