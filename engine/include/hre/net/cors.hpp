#pragma once

#include <string>
#include <map>

namespace hre::net {

class cors_checker {
public:
    cors_checker() = default;

    // Check if a cross-origin request is allowed
    bool is_cors_allowed(
        const std::string& origin,
        const std::string& request_url,
        const std::string& method,
        const std::map<std::string, std::string>& request_headers,
        const std::map<std::string, std::string>& response_headers
    ) const;

    // Check if credentials are allowed
    bool are_credentials_allowed(const std::map<std::string, std::string>& response_headers) const;

    // Resolve origin
    std::string resolve_origin(const std::string& url) const;

    // Simple vs preflight
    bool is_simple_method(const std::string& method) const;
    bool is_simple_header(const std::string& header) const;
    bool requires_preflight(const std::string& method, const std::map<std::string, std::string>& headers) const;

    // Handle preflight
    std::map<std::string, std::string> build_preflight_response(
        const std::string& origin,
        const std::string& request_method,
        const std::map<std::string, std::string>& request_headers
    ) const;

private:
    bool match_origin(const std::string& allowed, const std::string& origin) const;
    std::string get_header(const std::map<std::string, std::string>& headers, const std::string& name) const;
};

} // namespace hre::net
