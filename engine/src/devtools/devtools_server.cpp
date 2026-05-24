#include "hre/devtools/devtools_server.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace hre::devtools {

DevToolsServer& DevToolsServer::instance() {
    static DevToolsServer inst;
    return inst;
}

void DevToolsServer::register_console_callback(ConsoleCallback cb) {
    m_console_callbacks.push_back(cb);
}

void DevToolsServer::register_network_callback(NetworkCallback cb) {
    m_network_callbacks.push_back(cb);
}

void DevToolsServer::emit_console_message(const std::wstring& type, const std::wstring& text, const std::wstring& source, int line) {
    ConsoleMessage msg{ type, text, source, line };
    for (auto& cb : m_console_callbacks) {
        cb(msg);
    }
    // Also output to stdout for debugging
    std::wcout << L"[" << type << L"] " << text << std::endl;
}

void DevToolsServer::emit_network_request(const NetworkRequest& req) {
    for (auto& cb : m_network_callbacks) {
        cb(req);
    }
}

void DevToolsServer::emit_js_error(const std::wstring& message, const std::wstring& source, int line) {
    emit_console_message(L"error", message, source, line);
}

std::shared_ptr<DomNodeInfo> DevToolsServer::build_dom_tree(const dom::node* node) {
    if (!node) return nullptr;
    
    auto info = std::make_shared<DomNodeInfo>();
    
    if (node->type() == dom::node_type::element) {
        const auto* el = static_cast<const dom::element*>(node);
        info->tag_name = el->tag_name();
        info->id = el->id();
        info->classes = el->get_attribute(L"class");
        
        // Copy attributes
        // (In a full implementation, iterate all attributes)
    } else if (node->type() == dom::node_type::text) {
        const auto* txt = static_cast<const dom::text_node*>(node);
        info->tag_name = L"#text";
        info->inner_text = txt->text();
    }
    
    // Process children
    for (const auto& child : node->children()) {
        if (child) {
            auto child_info = build_dom_tree(child.get());
            if (child_info) {
                info->children.push_back(child_info);
            }
        }
    }
    
    return info;
}

std::wstring DomNodeInfo::to_json() const {
    std::wstringstream ss;
    ss << L"{";
    ss << L"\"tag\":\"" << tag_name << L"\",";
    if (!id.empty()) ss << L"\"id\":\"" << id << L"\",";
    if (!classes.empty()) ss << L"\"classes\":\"" << classes << L"\",";
    if (!inner_text.empty()) ss << L"\"text\":\"" << inner_text << L"\",";
    
    ss << L"\"children\":[";
    bool first = true;
    for (const auto& child : children) {
        if (!first) ss << L",";
        // Recursive call would be needed here in a full implementation
        ss << L"{}"; 
        first = false;
    }
    ss << L"]";
    
    ss << L"}";
    return ss.str();
}

std::wstring DevToolsServer::get_dom_as_json(const dom::node* root) {
    auto info = build_dom_tree(root);
    if (info) {
        return info->to_json();
    }
    return L"{}";
}

std::wstring DevToolsServer::evaluate_expression(const std::wstring& expression) {
    // In a real implementation, this would execute JS in the current context
    // and return the result as a JSON string.
    return L"\"Evaluation not implemented in this build\"";
}

} // namespace hre::devtools
