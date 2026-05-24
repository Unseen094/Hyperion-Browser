#pragma once

#include <string>
#include <vector>

namespace hre::security {

enum class cors_origin { same_origin, cross_origin, opaque };
enum class cors_result { allow, deny, need_preflight };

class cors_checker {
    std::string origin_;
public:
    explicit cors_checker(const std::string& origin) : origin_(origin) {}

    cors_result check_request(const std::string& method,
                              const std::vector<std::string>& headers,
                              const std::string& url,
                              bool with_credentials);

    cors_result check_preflight(const std::string& method,
                                const std::string& url,
                                const std::vector<std::string>& request_headers,
                                const std::string& ac_request_method,
                                const std::string& ac_request_headers);

    std::string generate_response_headers(cors_result r);

    static cors_origin classify_origin(const std::string& origin, const std::string& target);
    static bool origin_matches(const std::string& pattern, const std::string& origin);
};

} // namespace hre::security
