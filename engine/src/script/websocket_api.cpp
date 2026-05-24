#include "hre/script/websocket_api.hpp"
#include "hre/net/websocket.hpp"
#include <hjs/runtime/promise.hpp>
#include <hjs/runtime/object.hpp>
#include <hjs/vm/vm.hpp>
#include <thread>
#include <chrono>

namespace hre::script {

// A WebSocket object in JS
struct WebSocketObject {
    std::unique_ptr<hre::net::websocket> ws;
    std::string url;
    bool connected;
    
    WebSocketObject() : ws(std::make_unique<hre::net::websocket>()), connected(false) {}
};

// Global registry for WebSocket objects (stored as raw pointers to avoid unique_ptr copy issues)
static std::map<hjs::JSObject*, WebSocketObject*> g_websockets;

hjs::Value native_websocket_constructor(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) {
        auto p = std::make_shared<hjs::Promise>();
        p->reject(hjs::Value(L"Invalid URL"));
        return hjs::Value(p);
    }
    
    std::wstring url_w = args[0].as_string();
    std::string url(url_w.begin(), url_w.end());
    
    auto ws_obj = std::make_shared<hjs::JSObject>();
    auto* ws = new WebSocketObject();
    ws->url = url;
    
    // Store the WebSocketObject pointer in the JS object
    ws_obj->set_property(L"_ws", hjs::Value(static_cast<int64_t>(reinterpret_cast<uintptr_t>(ws))));
    
    // Methods
    ws_obj->set_property(L"connect", hjs::Value(std::make_shared<hjs::NativeFunction>(native_websocket_connect)));
    ws_obj->set_property(L"send", hjs::Value(std::make_shared<hjs::NativeFunction>(native_websocket_send)));
    ws_obj->set_property(L"close", hjs::Value(std::make_shared<hjs::NativeFunction>(native_websocket_close)));
    
    // Event handlers
    ws_obj->set_property(L"onopen", hjs::Value());
    ws_obj->set_property(L"onmessage", hjs::Value());
    ws_obj->set_property(L"onclose", hjs::Value());
    ws_obj->set_property(L"onerror", hjs::Value());
    
    // Store in global registry
    g_websockets[ws_obj.get()] = ws;
    
    // Return a Promise that resolves when connected
    auto promise = std::make_shared<hjs::Promise>();
    
    // Start connection in background
    std::thread([promise, ws_obj, ws]() {
        bool success = ws->ws->connect(ws->url);
        
        // Schedule on the main thread to resolve the promise
        // Simulate by calling the callback immediately
        if (success) {
            ws->connected = true;
            promise->resolve(hjs::Value(ws_obj));
            
            // Invoke onopen
            hjs::Value* onopen = ws_obj->get_property(L"onopen");
            if (onopen && onopen->is_object()) {
                // Invoke as function
                hjs::vm::VM vm;
                hjs::vm::Chunk chunk;
                int fn_idx = chunk.add_constant(*onopen);
                chunk.write((uint8_t)hjs::vm::OpCode::Constant);
                chunk.write((uint8_t)fn_idx);
                chunk.write((uint8_t)hjs::vm::OpCode::Call);
                chunk.write((uint8_t)0);
                chunk.write((uint8_t)hjs::vm::OpCode::Pop);
                chunk.write((uint8_t)hjs::vm::OpCode::Return);
                vm.interpret(chunk);
            }
        } else {
            promise->reject(hjs::Value(L"WebSocket connection failed"));
            
            // Invoke onerror
            hjs::Value* onerror = ws_obj->get_property(L"onerror");
            if (onerror && onerror->is_object()) {
                hjs::vm::VM vm;
                hjs::vm::Chunk chunk;
                int fn_idx = chunk.add_constant(*onerror);
                chunk.write((uint8_t)hjs::vm::OpCode::Constant);
                chunk.write((uint8_t)fn_idx);
                chunk.write((uint8_t)hjs::vm::OpCode::Call);
                chunk.write((uint8_t)0);
                chunk.write((uint8_t)hjs::vm::OpCode::Pop);
                chunk.write((uint8_t)hjs::vm::OpCode::Return);
                vm.interpret(chunk);
            }
        }
    }).detach();
    
    return hjs::Value(promise);
}

hjs::Value native_websocket_connect(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) {
        return hjs::Value();
    }
    
    // This method is not needed since connect is called in constructor
    // We'll just return a resolved promise
    auto promise = std::make_shared<hjs::Promise>();
    promise->resolve(hjs::Value());
    return hjs::Value(promise);
}

hjs::Value native_websocket_send(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) {
        return hjs::Value();
    }
    
    auto* obj = dynamic_cast<hjs::JSObject*>(receiver.as_object().get());
    if (!obj) return hjs::Value();
    
    hjs::Value* ws_ptr_val = obj->get_property(L"_ws");
    if (!ws_ptr_val || !ws_ptr_val->is_number()) return hjs::Value();
    
    uintptr_t ptr = ws_ptr_val->as_number();
    auto* ws_obj = reinterpret_cast<WebSocketObject*>(ptr);
    
    std::wstring data_w = args[0].as_string();
    std::string data(data_w.begin(), data_w.end());
    
    bool success = ws_obj->ws->send(data);
    
    auto promise = std::make_shared<hjs::Promise>();
    if (success) {
        promise->resolve(hjs::Value());
    } else {
        promise->reject(hjs::Value(L"Failed to send"));
    }
    
    return hjs::Value(promise);
}

hjs::Value native_websocket_close(hjs::Value receiver, int, hjs::Value*) {
    auto* obj = dynamic_cast<hjs::JSObject*>(receiver.as_object().get());
    if (!obj) return hjs::Value();
    
    hjs::Value* ws_ptr_val = obj->get_property(L"_ws");
    if (!ws_ptr_val || !ws_ptr_val->is_number()) return hjs::Value();
    
    uintptr_t ptr = ws_ptr_val->as_number();
    auto* ws_obj = reinterpret_cast<WebSocketObject*>(ptr);
    
    ws_obj->ws->close();
    ws_obj->connected = false;
    
    // Invoke onclose
    hjs::Value* onclose = obj->get_property(L"onclose");
    if (onclose && onclose->is_object()) {
        hjs::vm::VM vm;
        hjs::vm::Chunk chunk;
        int fn_idx = chunk.add_constant(*onclose);
        chunk.write((uint8_t)hjs::vm::OpCode::Constant);
        chunk.write((uint8_t)fn_idx);
        chunk.write((uint8_t)hjs::vm::OpCode::Call);
        chunk.write((uint8_t)0);
        chunk.write((uint8_t)hjs::vm::OpCode::Pop);
        chunk.write((uint8_t)hjs::vm::OpCode::Return);
        vm.interpret(chunk);
    }
    
    auto promise = std::make_shared<hjs::Promise>();
    promise->resolve(hjs::Value());
    return hjs::Value(promise);
}

} // namespace hre::script
