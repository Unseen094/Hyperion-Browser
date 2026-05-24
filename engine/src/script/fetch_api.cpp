#include "hre/script/fetch_api.hpp"
#include "hre/net/request.hpp"
#include "hre/storage/local_storage.hpp"
#include "hre/storage/session_storage.hpp"
#include <hjs/runtime/promise.hpp>
#include <thread>
#include <codecvt>
#include <locale>

namespace hre::script {

// Helper to convert wstring to utf8
static std::string wstring_to_utf8(const std::wstring& ws) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws);
#pragma warning(pop)
}

// Helper to create a resolved Promise in the current VM context
static hjs::Value create_resolved_promise(hjs::Value value) {
    auto promise_obj = std::make_shared<hjs::Promise>();
    promise_obj->resolve(value);
    // Wrap in JSObject for VM
    return hjs::Value(std::static_pointer_cast<hjs::JSObject>(promise_obj));
}

hjs::Value native_fetch(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) {
        // Return a rejected promise if args invalid
        auto promise_obj = std::make_shared<hjs::Promise>();
        promise_obj->reject(hjs::Value(L"Invalid arguments"));
        return hjs::Value(std::static_pointer_cast<hjs::JSObject>(promise_obj));
    }

    std::wstring url = args[0].as_string();

    // Create a Promise object
    auto promise_obj = std::make_shared<hjs::Promise>();

    // Run network request in a separate thread
    std::thread([promise_obj, url]() {
        std::wstring result = hre::net::request::fetch(url);
        // Resolve the promise on the main thread (simulated)
        // In a real engine, this would schedule a task on the event loop
        promise_obj->resolve(hjs::Value(result));
    }).detach();

    return hjs::Value(std::static_pointer_cast<hjs::JSObject>(promise_obj));
}

// ----- localStorage ---------------------------------------------------------

hjs::Value native_local_storage_get(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) return hjs::Value();
    std::wstring key = args[0].as_string();
    return hjs::Value(hre::storage::local_storage::get_item(key));
}

hjs::Value native_local_storage_set(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 2 || !args[0].is_string() || !args[1].is_string()) return hjs::Value();
    hre::storage::local_storage::set_item(args[0].as_string(), args[1].as_string());
    return hjs::Value();
}

hjs::Value native_local_storage_remove(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) return hjs::Value();
    hre::storage::local_storage::remove_item(args[0].as_string());
    return hjs::Value();
}

hjs::Value native_local_storage_clear(hjs::Value, int, hjs::Value*) {
    hre::storage::local_storage::clear();
    return hjs::Value();
}

// ----- sessionStorage -------------------------------------------------------

hjs::Value native_session_storage_get(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) return hjs::Value();
    std::wstring key = args[0].as_string();
    return hjs::Value(hre::storage::session_storage::get_item(key));
}

hjs::Value native_session_storage_set(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 2 || !args[0].is_string() || !args[1].is_string()) return hjs::Value();
    hre::storage::session_storage::set_item(args[0].as_string(), args[1].as_string());
    return hjs::Value();
}

hjs::Value native_session_storage_remove(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) return hjs::Value();
    hre::storage::session_storage::remove_item(args[0].as_string());
    return hjs::Value();
}

hjs::Value native_session_storage_clear(hjs::Value, int, hjs::Value*) {
    hre::storage::session_storage::clear();
    return hjs::Value();
}

} // namespace hre::script
