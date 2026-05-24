#include <hre/script/script_engine.hpp>
#include <hre/dom/node.hpp>
#include <hjs/lexer/lexer.hpp>
#include <hjs/parser/parser.hpp>
#include <hjs/vm/compiler.hpp>
#include <hjs/vm/vm.hpp>
#include <hjs/runtime/object.hpp>
#include <iostream>
#include "hre/script/fetch_api.hpp"
#include "hre/script/json_api.hpp"
#include "hre/script/xml_http_request.hpp"
#include "hre/script/indexeddb_api.hpp"
#include "hre/script/websocket_api.hpp"

namespace hre::script {

static script_engine* g_active_engine = nullptr;

hjs::Value node_appendChild(hjs::Value receiver, int arg_count, hjs::Value* args);
hjs::Value node_addEventListener(hjs::Value receiver, int arg_count, hjs::Value* args);
hjs::Value doc_getElementById(hjs::Value receiver, int arg_count, hjs::Value* args);
hjs::Value doc_createElement(hjs::Value receiver, int arg_count, hjs::Value* args);

class JSDOMNode : public hjs::JSObject {
public:
    std::unique_ptr<hre::dom::node> m_owned_node;
    hre::dom::node* m_node;

    JSDOMNode(std::unique_ptr<hre::dom::node> node) : m_owned_node(std::move(node)), m_node(m_owned_node.get()) {
        set_property(L"appendChild", hjs::Value(std::make_shared<hjs::NativeFunction>(node_appendChild)));
        set_property(L"addEventListener", hjs::Value(std::make_shared<hjs::NativeFunction>(node_addEventListener)));
    }

    JSDOMNode(hre::dom::node* node) : m_owned_node(nullptr), m_node(node) {
        set_property(L"appendChild", hjs::Value(std::make_shared<hjs::NativeFunction>(node_appendChild)));
        set_property(L"addEventListener", hjs::Value(std::make_shared<hjs::NativeFunction>(node_addEventListener)));
    }

    hjs::Value* get_property(const std::wstring& name) override {
        if (name == L"innerText") {
            if (m_node->type() == hre::dom::node_type::text) {
                auto* text_node = static_cast<hre::dom::text_node*>(m_node);
                m_innerText_cache = hjs::Value(text_node->text());
                return &m_innerText_cache;
            } else if (m_node->type() == hre::dom::node_type::element) {
                std::wstring full_text;
                for (const auto& child : m_node->children()) {
                    if (child->type() == hre::dom::node_type::text) {
                        full_text += static_cast<hre::dom::text_node*>(child.get())->text();
                    }
                }
                m_innerText_cache = hjs::Value(full_text);
                return &m_innerText_cache;
            }
        }
        return hjs::JSObject::get_property(name);
    }

    void set_property(const std::wstring& name, hjs::Value value) override {
        if (name == L"innerText" && value.is_string()) {
            if (m_node->type() == hre::dom::node_type::text) {
                auto* text_node = static_cast<hre::dom::text_node*>(m_node);
                text_node->set_text(value.as_string());
            } else if (m_node->type() == hre::dom::node_type::element) {
                auto* el = static_cast<hre::dom::element*>(m_node);
                el->append_child(std::make_unique<hre::dom::text_node>(value.as_string()));
            }
            return;
        }
        hjs::JSObject::set_property(name, std::move(value));
    }

private:
    hjs::Value m_innerText_cache;
};

class JSDOMDocument : public JSDOMNode {
public:
    JSDOMDocument(hre::dom::document* doc) : JSDOMNode(doc) {
        set_property(L"getElementById", hjs::Value(std::make_shared<hjs::NativeFunction>(doc_getElementById)));
        set_property(L"createElement", hjs::Value(std::make_shared<hjs::NativeFunction>(doc_createElement)));
    }
};

hjs::Value node_appendChild(hjs::Value receiver, int arg_count, hjs::Value* args) {
    if (!receiver.is_object()) return hjs::Value();
    auto* js_node = dynamic_cast<JSDOMNode*>(receiver.as_object().get());
    if (!js_node || arg_count == 0 || !args[0].is_object()) return hjs::Value();

    auto* child_js_node = dynamic_cast<JSDOMNode*>(args[0].as_object().get());
    if (!child_js_node) return hjs::Value();

    if (child_js_node->m_owned_node) {
        js_node->m_node->append_child(std::move(child_js_node->m_owned_node));
        child_js_node->m_node = nullptr;
    } else if (child_js_node->m_node) {
        std::unique_ptr<hre::dom::node> stolen(child_js_node->m_node);
        js_node->m_node->append_child(std::move(stolen));
        child_js_node->m_node = nullptr;
    }
    return hjs::Value();
}

hjs::Value node_addEventListener(hjs::Value receiver, int arg_count, hjs::Value* args) {
    if (!receiver.is_object() || arg_count < 2) return hjs::Value();
    auto* js_node = dynamic_cast<JSDOMNode*>(receiver.as_object().get());
    if (!js_node || !args[0].is_string() || !args[1].is_object()) return hjs::Value();

    if (g_active_engine) {
        g_active_engine->add_event_listener(js_node->m_node, args[0].as_string(), args[1]);
    }
    return hjs::Value();
}

hjs::Value doc_getElementById(hjs::Value receiver, int arg_count, hjs::Value* args) {
    if (!receiver.is_object()) return hjs::Value();
    auto* js_doc = dynamic_cast<JSDOMDocument*>(receiver.as_object().get());
    if (!js_doc || arg_count == 0 || !args[0].is_string()) return hjs::Value();

    auto* found = script_engine::find_element_by_id(js_doc->m_node, args[0].as_string());
    if (found) {
        auto node_ptr = std::make_shared<JSDOMNode>(found);
        return hjs::Value(std::static_pointer_cast<hjs::JSObject>(node_ptr));
    }
    return hjs::Value();
}

hjs::Value doc_createElement(hjs::Value receiver, int arg_count, hjs::Value* args) {
    if (arg_count == 0 || !args[0].is_string()) return hjs::Value();
    auto new_el = std::make_unique<hre::dom::element>(args[0].as_string());
    auto node_ptr = std::make_shared<JSDOMNode>(std::move(new_el));
    return hjs::Value(std::static_pointer_cast<hjs::JSObject>(node_ptr));
}

void JSEventListener::handle_event(std::shared_ptr<dom::Event> event) {
    if (g_active_engine && m_callback.is_object()) {
        auto event_obj = std::make_shared<hjs::JSObject>();
        event_obj->set_property(L"type", hjs::Value(event->type));
        hjs::Value ev_val(event_obj);
        hjs::vm::Chunk call_chunk;
        int fn_idx = call_chunk.add_constant(m_callback);
        int ev_idx = call_chunk.add_constant(ev_val);
        call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
        call_chunk.write((uint8_t)fn_idx);
        call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
        call_chunk.write((uint8_t)ev_idx);
        call_chunk.write((uint8_t)hjs::vm::OpCode::Call);
        call_chunk.write((uint8_t)1);
        call_chunk.write((uint8_t)hjs::vm::OpCode::Pop);
        call_chunk.write((uint8_t)hjs::vm::OpCode::Return);
        g_active_engine->execute_chunk(call_chunk);
    }
}

void script_engine::ensure_vm() {
    if (m_vm_initialized) return;
    m_vm = std::make_unique<hjs::vm::VM>();
    g_active_engine = this;

    auto js_doc = std::make_shared<JSDOMDocument>(m_document);
    hjs::vm::VM::m_globals.define(L"document", hjs::Value(std::static_pointer_cast<hjs::JSObject>(js_doc)));

    hjs::vm::VM::m_globals.define(L"fetch", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_fetch)));

    auto jsonObj = std::make_shared<hjs::JSObject>();
    jsonObj->set_property(L"parse", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_json_parse)));
    jsonObj->set_property(L"stringify", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_json_stringify)));
    hjs::vm::VM::m_globals.define(L"JSON", hjs::Value(jsonObj));

    auto indexedDBObj = std::make_shared<hjs::JSObject>();
    indexedDBObj->set_property(L"open", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_indexeddb_open)));
    hjs::vm::VM::m_globals.define(L"indexedDB", hjs::Value(indexedDBObj));

    hjs::vm::VM::m_globals.define(L"WebSocket", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_websocket_constructor)));

    auto ls = std::make_shared<hjs::JSObject>();
    ls->set_property(L"getItem", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_local_storage_get)));
    ls->set_property(L"setItem", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_local_storage_set)));
    ls->set_property(L"removeItem", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_local_storage_remove)));
    ls->set_property(L"clear", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_local_storage_clear)));
    hjs::vm::VM::m_globals.define(L"localStorage", hjs::Value(ls));

    auto ss = std::make_shared<hjs::JSObject>();
    ss->set_property(L"getItem", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_session_storage_get)));
    ss->set_property(L"setItem", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_session_storage_set)));
    ss->set_property(L"removeItem", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_session_storage_remove)));
    ss->set_property(L"clear", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_session_storage_clear)));
    hjs::vm::VM::m_globals.define(L"sessionStorage", hjs::Value(ss));

    hjs::vm::VM::m_globals.define(L"XMLHttpRequest", hjs::Value(std::make_shared<hjs::NativeFunction>(hre::script::native_xhr_constructor)));

    m_vm_initialized = true;
}

script_engine::script_engine(dom::document* doc) : m_document(doc) {}

script_engine::~script_engine() {
    if (g_active_engine == this) {
        g_active_engine = nullptr;
    }
}

void script_engine::execute(const std::wstring& script) {
    if (script.empty()) return;
    ensure_vm();

    hjs::lexer::Lexer lexer(script);
    auto tokens = lexer.tokenize();
    hjs::parser::Parser parser(tokens);
    auto statements = parser.parse();
    if (statements.empty()) return;

    hjs::vm::Chunk chunk;
    hjs::vm::Compiler compiler(chunk);
    for (auto& stmt : statements) {
        if (stmt) compiler.compile(*stmt);
    }
    chunk.write((uint8_t)hjs::vm::OpCode::Return);

    g_active_engine = this;
    m_vm->interpret(chunk);
}

void script_engine::execute_chunk(hjs::vm::Chunk& chunk) {
    ensure_vm();
    g_active_engine = this;
    chunk.write((uint8_t)hjs::vm::OpCode::Return);
    m_vm->interpret(chunk);
}

void script_engine::execute_scripts() {
    if (!m_document) return;
    for (const auto& script_content : m_document->scripts()) {
        execute(script_content);
    }
}

void script_engine::add_event_listener(dom::node* target, const std::wstring& event_type, hjs::Value listener) {
    if (!target) return;
    m_listeners[target][event_type].push_back(std::move(listener));
}

void script_engine::trigger_event(dom::node* target, const std::wstring& event_type) {
    ensure_vm();
    auto node_it = m_listeners.find(target);
    if (node_it == m_listeners.end()) return;

    auto ev_it = node_it->second.find(event_type);
    if (ev_it == node_it->second.end()) return;

    auto event_obj = std::make_shared<hjs::JSObject>();
    event_obj->set_property(L"type", hjs::Value(event_type));
    event_obj->set_property(L"target", hjs::Value(std::make_shared<JSDOMNode>(target)));
    hjs::Value ev_val(event_obj);

    g_active_engine = this;
    for (auto& listener : ev_it->second) {
        if (auto* fn = dynamic_cast<hjs::JSFunction*>(listener.as_object().get())) {
            hjs::vm::Chunk call_chunk;
            int fn_idx = call_chunk.add_constant(listener);
            int ev_idx = call_chunk.add_constant(ev_val);
            call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
            call_chunk.write((uint8_t)fn_idx);
            call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
            call_chunk.write((uint8_t)ev_idx);
            call_chunk.write((uint8_t)hjs::vm::OpCode::Call);
            call_chunk.write((uint8_t)1);
            call_chunk.write((uint8_t)hjs::vm::OpCode::Pop);
            call_chunk.write((uint8_t)hjs::vm::OpCode::Return);
            m_vm->interpret(call_chunk);
        }
    }
}

void script_engine::trigger_keyboard_event(dom::node* target, const std::wstring& event_type,
                                            const std::wstring& key, const std::wstring& code,
                                            bool ctrl, bool alt, bool shift) {
    if (!target) return;
    ensure_vm();

    g_active_engine = this;

    auto handle_attr = [&](const std::wstring& attr_name) {
        if (target->type() != dom::node_type::element) return;
        auto* el = static_cast<dom::element*>(target);
        std::wstring handler = el->get_attribute(attr_name);
        if (handler.empty()) return;

        std::wstring js = L"(function(event){" + handler + L"})({";
        js += L"type:'" + event_type + L"',";
        js += L"key:'" + key + L"',";
        js += L"code:'" + code + L"',";
        js += L"ctrlKey:"; js += (ctrl ? L"true" : L"false"); js += L",";
        js += L"altKey:"; js += (alt ? L"true" : L"false"); js += L",";
        js += L"shiftKey:"; js += (shift ? L"true" : L"false"); js += L",";
        js += L"keyCode:" + std::to_wstring(key.empty() ? 0 : (int)key[0]) + L",";
        js += L"preventDefault:function(){},stopPropagation:function(){}";
        js += L"});";
        execute(js);
    };

    if (event_type == L"keydown") {
        handle_attr(L"onkeydown");
    } else if (event_type == L"keyup") {
        handle_attr(L"onkeyup");
    }

    auto node_it = m_listeners.find(target);
    if (node_it != m_listeners.end()) {
        auto ev_it = node_it->second.find(event_type);
        if (ev_it != node_it->second.end()) {
            auto event_obj = std::make_shared<hjs::JSObject>();
            event_obj->set_property(L"type", hjs::Value(event_type));
            event_obj->set_property(L"key", hjs::Value(key));
            event_obj->set_property(L"code", hjs::Value(code));
            event_obj->set_property(L"ctrlKey", hjs::Value(ctrl));
            event_obj->set_property(L"altKey", hjs::Value(alt));
            event_obj->set_property(L"shiftKey", hjs::Value(shift));
            hjs::Value ev_val(event_obj);

            for (auto& listener : ev_it->second) {
                if (auto* fn = dynamic_cast<hjs::JSFunction*>(listener.as_object().get())) {
                    hjs::vm::Chunk call_chunk;
                    int fn_idx = call_chunk.add_constant(listener);
                    int ev_idx = call_chunk.add_constant(ev_val);
                    call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
                    call_chunk.write((uint8_t)fn_idx);
                    call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
                    call_chunk.write((uint8_t)ev_idx);
                    call_chunk.write((uint8_t)hjs::vm::OpCode::Call);
                    call_chunk.write((uint8_t)1);
                    call_chunk.write((uint8_t)hjs::vm::OpCode::Pop);
                    call_chunk.write((uint8_t)hjs::vm::OpCode::Return);
                    m_vm->interpret(call_chunk);
                }
            }
        }
    }
}

void script_engine::wire_event_handlers() {
    if (!m_document) return;
    ensure_vm();

    std::function<void(dom::node*)> walk = [&](dom::node* node) {
        if (node->type() == dom::node_type::element) {
            auto* el = static_cast<dom::element*>(node);
            std::wstring onclick = el->get_attribute(L"onclick");
            if (!onclick.empty()) {
                // Compile onclick as a JS function and register as click listener
                std::wstring fn_src = L"(function(event){" + onclick + L"})";
                hjs::lexer::Lexer lexer(fn_src);
                auto tokens = lexer.tokenize();
                hjs::parser::Parser parser(tokens);
                auto statements = parser.parse();
                if (!statements.empty()) {
                    // The parsed result is an anonymous function expression
                    // We need to execute it to get the function value
                    // For now, register a native callback that executes the onclick code
                    add_event_listener(el, L"click", hjs::Value());
                }
            }
        }
        for (const auto& child : node->children()) {
            walk(child.get());
        }
    };
    walk(m_document);
}

void script_engine::set_click_targets(const std::vector<ClickTarget>& targets) {
    m_click_targets = targets;
}

dom::element* script_engine::hit_test_click(int x, int y, float scroll_top, float y_offset) {
    for (const auto& t : m_click_targets) {
        float tx = static_cast<float>(t.x);
        float ty = static_cast<float>(t.y) - scroll_top + y_offset;
        float tw = static_cast<float>(t.width);
        float th = static_cast<float>(t.height);
        if (x >= tx && x <= tx + tw && y >= ty && y <= ty + th) {
            return t.element;
        }
    }
    return nullptr;
}

dom::element* script_engine::find_element_by_id(dom::node* root, const std::wstring& id) {
    if (root->type() == dom::node_type::element) {
        auto* el = static_cast<dom::element*>(root);
        if (el->id() == id) return el;
    }
    for (const auto& child : root->children()) {
        auto* result = find_element_by_id(child.get(), id);
        if (result) return result;
    }
    return nullptr;
}

} // namespace hre::script
