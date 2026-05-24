#include <hre/devtools/devtools_cdp.hpp>
#include <sstream>
#include <algorithm>

namespace hre::devtools {

cdp_session::cdp_session() {
    register_domain({"DOM", [this](const std::string& p) {
        dom_get_document();
        return R"({"result":{"nodeId":1,"nodeType":9,"nodeName":"#document"}})";
    }});
    register_domain({"CSS", [this](const std::string& p) {
        css_get_styles();
        return R"({"result":{"matchedCSSRules":[]}})";
    }});
    register_domain({"Network", [this](const std::string& p) {
        network_enable();
        return R"({"result":{}})";
    }});
    register_domain({"Console", [this](const std::string& p) {
        console_enable();
        return R"({"result":{}})";
    }});
    register_domain({"Runtime", [this](const std::string& p) {
        return R"({"result":{"result":{"type":"undefined","value":"undefined"}}})";
    }});
    register_domain({"Page", [this](const std::string& p) {
        return R"({"result":{"frameId":"1"}})";
    }});
}

void cdp_session::register_domain(cdp_domain d) {
    handlers_[d.name] = d.handler;
}

std::string cdp_session::build_response(const std::string& id, const std::string& result) {
    std::ostringstream os;
    os << R"({"id":)" << id << R"(,"result":)" << result << "}";
    return os.str();
}

std::string cdp_session::send_command(const std::string& domain, const std::string& method, const std::string& params) {
    auto it = handlers_.find(domain);
    if (it != handlers_.end()) {
        return it->second(params);
    }
    std::string id_str = std::to_string(msg_id_++);
    return build_response(id_str, R"({"type":"undefined"})");
}

std::string cdp_session::handle_message(const std::string& json_msg) {
    std::string id = "0";
    std::string method;
    std::string params = "{}";

    auto id_pos = json_msg.find("\"id\"");
    if (id_pos != std::string::npos) {
        auto start = json_msg.find(':', id_pos + 3);
        if (start != std::string::npos) {
            auto end = json_msg.find_first_of(",}", start + 1);
            if (end != std::string::npos) {
                id = json_msg.substr(start + 1, end - start - 1);
                id.erase(std::remove(id.begin(), id.end(), ' '), id.end());
            }
        }
    }

    auto method_pos = json_msg.find("\"method\"");
    if (method_pos != std::string::npos) {
        auto start = json_msg.find(':', method_pos + 7);
        if (start != std::string::npos) {
            auto end = json_msg.find('"', start + 2);
            if (end != std::string::npos) {
                method = json_msg.substr(start + 2, end - start - 2);
            }
        }
    }

    auto params_pos = json_msg.find("\"params\"");
    if (params_pos != std::string::npos) {
        auto start = json_msg.find('{', params_pos + 7);
        if (start != std::string::npos) {
            int depth = 0;
            size_t end = start;
            for (; end < json_msg.size(); ++end) {
                if (json_msg[end] == '{') ++depth;
                if (json_msg[end] == '}') { --depth; if (depth == 0) break; }
            }
            if (depth == 0) params = json_msg.substr(start, end - start + 1);
        }
    }

    if (method.empty()) {
        return build_response(id, R"({"type":"undefined"})");
    }

    auto dot = method.find('.');
    std::string domain = (dot != std::string::npos) ? method.substr(0, dot) : method;

    return send_command(domain, method, params);
}

void cdp_session::dom_get_document() {
}

void cdp_session::css_get_styles() {
}

void cdp_session::network_enable() {
}

void cdp_session::console_enable() {
}

} // namespace hre::devtools
