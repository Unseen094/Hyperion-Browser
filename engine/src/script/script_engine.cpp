#include <hre/script/script_engine.hpp>
#include <hre/dom/node.hpp>
#include <hjs/lexer/lexer.hpp>
#include <hjs/parser/parser.hpp>
#include <hjs/vm/compiler.hpp>
#include <hjs/vm/vm.hpp>
#include <hjs/runtime/object.hpp>
#include <iostream>

namespace hre::script {

// Global engine instance for event dispatch
static script_engine* g_active_engine = nullptr;

// Forward declarations of native functions
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
                std::wstring full_text = L"";
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
        auto* child_raw = child_js_node->m_owned_node.get();
        js_node->m_node->append_child(std::move(child_js_node->m_owned_node));
        child_js_node->m_node = nullptr;
    } else if (child_js_node->m_node) {
        auto* child_raw = child_js_node->m_node;
        std::unique_ptr<hre::dom::node> stolen(child_raw);
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

script_engine::script_engine(dom::document* doc) : m_document(doc) {
    g_active_engine = this;
    auto js_doc = std::make_shared<JSDOMDocument>(m_document);
    hjs::vm::VM::m_globals.define(L"document", hjs::Value(std::static_pointer_cast<hjs::JSObject>(js_doc)));
}

void script_engine::execute(const std::wstring& script) {
    if (script.empty()) return;
    std::wcout << L"[Script Engine] Executing DOM Script..." << std::endl;

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
    hjs::vm::VM vm;
    vm.interpret(chunk);
}

void script_engine::add_event_listener(dom::node* target, const std::wstring& event_type, hjs::Value listener) {
    m_listeners[target][event_type].push_back(std::move(listener));
}

void script_engine::trigger_event(dom::node* target, const std::wstring& event_type) {
    auto node_it = m_listeners.find(target);
    if (node_it == m_listeners.end()) return;

    auto ev_it = node_it->second.find(event_type);
    if (ev_it == node_it->second.end()) return;

    // Create an Event object (minimal)
    auto event_obj = std::make_shared<hjs::JSObject>();
    event_obj->set_property(L"type", hjs::Value(event_type));
    event_obj->set_property(L"target", hjs::Value(std::make_shared<JSDOMNode>(target)));

    hjs::Value ev_val(event_obj);

    // Prepare a temporary chunk to call the listeners
    for (auto& listener : ev_it->second) {
        if (auto* fn = dynamic_cast<hjs::JSFunction*>(listener.as_object().get())) {
            // Setup a call chunk
            hjs::vm::Chunk call_chunk;
            int fn_idx = call_chunk.add_constant(listener);
            int ev_idx = call_chunk.add_constant(ev_val);

            call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
            call_chunk.write((uint8_t)fn_idx);
            
            call_chunk.write((uint8_t)hjs::vm::OpCode::Constant);
            call_chunk.write((uint8_t)ev_idx);

            call_chunk.write((uint8_t)hjs::vm::OpCode::Call);
            call_chunk.write((uint8_t)1); // 1 argument

            call_chunk.write((uint8_t)hjs::vm::OpCode::Pop);
            call_chunk.write((uint8_t)hjs::vm::OpCode::Return);

            g_active_engine = this;
            hjs::vm::VM vm;
            vm.interpret(call_chunk);
        }
    }
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
