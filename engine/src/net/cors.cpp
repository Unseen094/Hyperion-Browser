#include "hre/net/cors.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace hre::net {

static std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
    return r;
}

std::string cors_checker::get_header(const std::map<std::string, std::string>& headers, const std::string& name) const {
    std::string lower = to_lower(name);
    for (auto& [k, v] : headers) {
        if (to_lower(k) == lower) return v;
    }
    return "";
}

std::string cors_checker::resolve_origin(const std::string& url) const {
    size_t proto_end = url.find("://");
    if (proto_end == std::string::npos) return url;
    size_t path_start = url.find('/', proto_end + 3);
    if (path_start == std::string::npos) return url;
    return url.substr(0, path_start);
}

bool cors_checker::is_simple_method(const std::string& method) const {
    std::string m = to_lower(method);
    return m == "get" || m == "head" || m == "post";
}

bool cors_checker::is_simple_header(const std::string& header) const {
    std::string h = to_lower(header);
    return h == "accept" || h == "accept-language" || h == "content-language" ||
           h == "content-type" || h == "range" || h == "dpr" || h == "downlink" ||
           h == "save-data" || h == "viewport-width" || h == "width";
}

bool cors_checker::requires_preflight(const std::string& method, const std::map<std::string, std::string>& headers) const {
    if (!is_simple_method(method)) return true;

    for (auto& [key, _] : headers) {
        if (!is_simple_header(key)) return true;
    }

    // Check Content-Type
    auto ct = get_header(headers, "Content-Type");
    if (!ct.empty()) {
        std::string lower_ct = to_lower(ct);
        if (lower_ct != "application/x-www-form-urlencoded" &&
            lower_ct != "multipart/form-data" &&
            lower_ct != "text/plain") {
            return true;
        }
    }

    return false;
}

bool cors_checker::match_origin(const std::string& allowed, const std::string& origin) const {
    if (allowed == "*") return true;
    if (allowed == origin) return true;
    return false;
}

bool cors_checker::is_cors_allowed(
    const std::string& origin,
    const std::string& request_url,
    const std::string& method,
    const std::map<std::string, std::string>& request_headers,
    const std::map<std::string, std::string>& response_headers
) const {
    // Same-origin check
    std::string request_origin = resolve_origin(request_url);
    if (origin == request_origin) return true;

    // Cross-origin: check ACAO header
    std::string acao = get_header(response_headers, "Access-Control-Allow-Origin");
    if (!match_origin(acao, origin)) return false;

    // If credentials, check Vary: Origin
    bool has_credentials = get_header(request_headers, "Authorization") != "" ||
                           get_header(request_headers, "Cookie") != "";
    if (has_credentials) {
        bool credentials_allowed = are_credentials_allowed(response_headers);
        if (!credentials_allowed) return false;
        if (acao == "*") return false; // Credentials require specific origin
    }

    // Check allowed methods
    std::string acam = get_header(response_headers, "Access-Control-Allow-Methods");
    if (!acam.empty()) {
        std::istringstream iss(acam);
        std::string token;
        bool method_allowed = false;
        while (iss >> token) {
            if (to_lower(token) == to_lower(method)) {
                method_allowed = true;
                break;
            }
        }
        if (!method_allowed) return false;
    }

    // Check allowed headers
    std::string acah = get_header(response_headers, "Access-Control-Allow-Headers");
    if (!acah.empty()) {
        for (auto& [key, _] : request_headers) {
            if (is_simple_header(key)) continue;
            if (acah.find(key) == std::string::npos && acah != "*") return false;
        }
    }

    return true;
}

bool cors_checker::are_credentials_allowed(const std::map<std::string, std::string>& response_headers) const {
    std::string acac = get_header(response_headers, "Access-Control-Allow-Credentials");
    return to_lower(acac) == "true";
}

std::map<std::string, std::string> cors_checker::build_preflight_response(
    const std::string& origin,
    const std::string& request_method,
    const std::map<std::string, std::string>& request_headers
) const {
    std::map<std::string, std::string> resp;

    resp["Access-Control-Allow-Origin"] = origin;
    resp["Access-Control-Allow-Methods"] = request_method + ", GET, HEAD, POST, PUT, DELETE, PATCH";
    resp["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Requested-With";
    resp["Access-Control-Max-Age"] = "86400";
    resp["Vary"] = "Origin";

    return resp;
}

} // namespace hre::net
