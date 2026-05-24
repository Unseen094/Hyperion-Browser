#pragma once

#include <string>
#include <unordered_map>
#include <functional>

namespace hre::devtools {

struct cdp_domain {
    std::string name;
    std::function<std::string(const std::string&)> handler;
};

class cdp_session {
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> handlers_;
    int msg_id_ = 1;
public:
    cdp_session();
    std::string handle_message(const std::string& json_msg);
    void register_domain(cdp_domain d);
    std::string send_command(const std::string& domain, const std::string& method, const std::string& params);

    void dom_get_document();
    void css_get_styles();
    void network_enable();
    void console_enable();

    std::string build_response(const std::string& id, const std::string& result);
};

} // namespace hre::devtools
