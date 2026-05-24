#include <hre/security/cors_checker.hpp>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace hre::security {

static std::string to_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

cors_origin cors_checker::classify_origin(const std::string& origin, const std::string& target) {
    if (origin.empty() || target.empty()) return cors_origin::opaque;
    if (origin == "null" || target == "null") return cors_origin::opaque;
    return (origin == target) ? cors_origin::same_origin : cors_origin::cross_origin;
}

bool cors_checker::origin_matches(const std::string& pattern, const std::string& origin) {
    if (pattern == "*") return true;
    if (pattern == origin) return true;

    size_t ws = pattern.find("://");
    size_t wo = origin.find("://");
    if (ws == std::string::npos || wo == std::string::npos) return false;

    std::string scheme_p = pattern.substr(0, ws);
    std::string scheme_o = origin.substr(0, wo);
    if (scheme_p != scheme_o) return false;

    std::string rest_p = pattern.substr(ws + 3);
    std::string rest_o = origin.substr(wo + 3);

    size_t ps_p = rest_p.find('/');
    size_t ps_o = rest_o.find('/');
    std::string host_p = (ps_p == std::string::npos) ? rest_p : rest_p.substr(0, ps_p);
    std::string host_o = (ps_o == std::string::npos) ? rest_o : rest_o.substr(0, ps_o);

    if (host_p.find('*') != std::string::npos) {
        size_t ast = host_p.find('*');
        std::string prefix = host_p.substr(0, ast);
        std::string suffix = host_p.substr(ast + 1);
        if (host_o.size() < prefix.size() + suffix.size()) return false;
        if (host_o.substr(0, prefix.size()) != prefix) return false;
        if (host_o.substr(host_o.size() - suffix.size()) != suffix) return false;
        return true;
    }

    return host_p == host_o;
}

cors_result cors_checker::check_request(const std::string& method,
                                         const std::vector<std::string>& headers,
                                         const std::string& url,
                                         bool with_credentials) {
    std::string method_u = to_lower(method);
    bool simple_method = (method_u == "get" || method_u == "head" || method_u == "post");

    static const char* simple_headers[] = {
        "accept", "accept-language", "content-language",
        "content-type"
    };
    bool simple_headers_all = true;
    for (const auto& h : headers) {
        std::string hl = to_lower(h);
        bool found = false;
        for (const auto& sh : simple_headers) {
            if (hl == sh) { found = true; break; }
        }
        if (hl.find("content-type") == 0) {
            if (hl.find("application/x-www-form-urlencoded") != std::string::npos ||
                hl.find("multipart/form-data") != std::string::npos ||
                hl.find("text/plain") != std::string::npos) {
                found = true;
            }
        }
        if (!found) { simple_headers_all = false; break; }
    }

    if (simple_method && simple_headers_all && !with_credentials) {
        return cors_result::allow;
    }

    return cors_result::need_preflight;
}

cors_result cors_checker::check_preflight(const std::string& method,
                                           const std::string& url,
                                           const std::vector<std::string>& request_headers,
                                           const std::string& ac_request_method,
                                           const std::string& ac_request_headers) {
    if (ac_request_method.empty()) return cors_result::deny;

    std::string preq_method = to_lower(ac_request_method);
    std::string req_method = to_lower(method);

    if (!req_method.empty() && req_method != preq_method) return cors_result::deny;

    auto preq_headers = split(ac_request_headers, ',');
    for (const auto& rh : request_headers) {
        std::string rl = to_lower(rh);
        bool found = false;
        for (const auto& ph : preq_headers) {
            if (to_lower(ph) == rl) { found = true; break; }
        }
        if (!found) return cors_result::deny;
    }

    return cors_result::allow;
}

std::string cors_checker::generate_response_headers(cors_result r) {
    std::ostringstream os;
    switch (r) {
        case cors_result::allow:
            os << "Access-Control-Allow-Origin: " << origin_ << "\r\n";
            os << "Access-Control-Allow-Credentials: true\r\n";
            break;
        case cors_result::deny:
            break;
        case cors_result::need_preflight:
            os << "Access-Control-Allow-Origin: " << origin_ << "\r\n";
            os << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH\r\n";
            os << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
            os << "Access-Control-Max-Age: 86400\r\n";
            break;
    }
    return os.str();
}

} // namespace hre::security
