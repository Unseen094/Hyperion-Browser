#include "hre/script/xml_http_request.hpp"
#include "hre/net/network_manager.hpp"
#include <hjs/runtime/object.hpp>
#include <hjs/core/value.hpp>
#include <mutex>

namespace hre::script {

// Forward declaration of a shared prototype
static std::shared_ptr<hjs::JSObject> xhr_prototype;
static std::once_flag xhr_proto_once;

static void init_xhr_prototype() {
    xhr_prototype = std::make_shared<hjs::JSObject>();
    xhr_prototype->set_property(L"open", hjs::Value(std::make_shared<hjs::NativeFunction>(native_xhr_open)));
    xhr_prototype->set_property(L"send", hjs::Value(std::make_shared<hjs::NativeFunction>(native_xhr_send)));
    // Event handler placeholder (onreadystatechange)
    xhr_prototype->set_property(L"onreadystatechange", hjs::Value());
}

hjs::Value native_xhr_constructor(hjs::Value, int argc, hjs::Value* args) {
    std::call_once(xhr_proto_once, init_xhr_prototype);
    auto xhr_obj = std::make_shared<hjs::JSObject>();
    xhr_obj->set_prototype(xhr_prototype);
    // Initialize properties per spec
    xhr_obj->set_property(L"readyState", hjs::Value(0));
    xhr_obj->set_property(L"status", hjs::Value(0));
    xhr_obj->set_property(L"responseText", hjs::Value(L""));
    xhr_obj->set_property(L"onreadystatechange", hjs::Value());
    return hjs::Value(xhr_obj);
}

hjs::Value native_xhr_open(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 2) return hjs::Value();
    // args[0]: method (store but not used), args[1]: URL
    if (!args[1].is_string()) return hjs::Value();
    std::wstring url = args[1].as_string();
    if (auto* obj = dynamic_cast<hjs::JSObject*>(receiver.as_object().get())) {
        obj->set_property(L"_url", hjs::Value(url));
        if (argc >= 1 && args[0].is_string()) {
            obj->set_property(L"_method", hjs::Value(args[0].as_string()));
        }
    }
    return hjs::Value();
}

hjs::Value native_xhr_send(hjs::Value receiver, int argc, hjs::Value* args) {
    if (auto* obj = dynamic_cast<hjs::JSObject*>(receiver.as_object().get())) {
        hjs::Value* url_val = obj->get_property(L"_url");
        if (!url_val || !url_val->is_string()) {
            obj->set_property(L"status", hjs::Value(0));
            obj->set_property(L"responseText", hjs::Value(L""));
            obj->set_property(L"readyState", hjs::Value(0));
            return hjs::Value();
        }
        std::wstring url = url_val->as_string();
        // Perform synchronous request using network manager (will follow HTTPS/HTTP)
        hre::net::network_manager nm;
        std::string response = nm.perform_request(url);
        // Populate XHR fields per spec
        obj->set_property(L"responseText", hjs::Value(std::wstring(response.begin(), response.end())));
        obj->set_property(L"status", hjs::Value(200));
        obj->set_property(L"readyState", hjs::Value(4));
        // Note: onreadystatechange handling omitted for brevity.
        // In a full implementation, the handler would be invoked here.
        (void)obj; // suppress unused variable warning

    }
    return hjs::Value();
}

} // namespace hre::script

