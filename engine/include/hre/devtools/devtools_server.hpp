#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include "hre/dom/node.hpp"

namespace hre::devtools {

// DevTools Event Types
enum class EventType {
    ConsoleMessage,
    NetworkRequest,
    DomUpdated,
    JsError
};

// Console Message Structure
struct ConsoleMessage {
    std::wstring type; // "log", "error", "warning", "info"
    std::wstring text;
    std::wstring source;
    int line_number;
};

// Network Request Structure
struct NetworkRequest {
    std::wstring url;
    std::wstring method;
    int status_code;
    std::string response_body;
    double duration_ms;
};

// DOM Node Representation for DevTools
struct DomNodeInfo {
    std::wstring tag_name;
    std::wstring id;
    std::wstring classes;
    std::map<std::wstring, std::wstring> attributes;
    std::vector<std::shared_ptr<DomNodeInfo>> children;
    std::wstring inner_text;

    std::wstring to_json() const;
};

// Callback types
using ConsoleCallback = std::function<void(const ConsoleMessage&)>;
using NetworkCallback = std::function<void(const NetworkRequest&)>;

class DevToolsServer {
public:
    static DevToolsServer& instance();

    // Registration
    void register_console_callback(ConsoleCallback cb);
    void register_network_callback(NetworkCallback cb);

    // Event emitters
    void emit_console_message(const std::wstring& type, const std::wstring& text, const std::wstring& source = L"", int line = 0);
    void emit_network_request(const NetworkRequest& req);
    void emit_js_error(const std::wstring& message, const std::wstring& source = L"", int line = 0);

    // DOM Inspection
    std::shared_ptr<DomNodeInfo> build_dom_tree(const dom::node* node);
    std::wstring get_dom_as_json(const dom::node* root);

    // Evaluation
    std::wstring evaluate_expression(const std::wstring& expression);

private:
    DevToolsServer() = default;
    
    std::vector<ConsoleCallback> m_console_callbacks;
    std::vector<NetworkCallback> m_network_callbacks;

    void dom_node_to_info(const dom::node* node, std::shared_ptr<DomNodeInfo> info);
};

} // namespace hre::devtools
