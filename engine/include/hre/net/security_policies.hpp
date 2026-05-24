#pragma once

#include <string>
#include <map>
#include <optional>

namespace hre::net {

enum class cross_origin_resource_policy {
    SAME_SITE,
    SAME_ORIGIN,
    CROSS_ORIGIN,
    NOT_SET
};

enum class cross_origin_embedder_policy {
    UNSAFE_NONE,
    REQUIRE_CORP,
    CREDENTIALLESS
};

enum class cross_origin_opener_policy {
    UNSAFE_NONE,
    SAME_ORIGIN,
    SAME_ORIGIN_ALLOW_POPUPS,
    RESTRICTED_PROPERTIES
};

enum class cross_origin_request_result {
    ALLOWED,
    BLOCKED_BY_CORP,
    BLOCKED_BY_COEP,
    BLOCKED_BY_COOP
};

struct security_policy_config {
    cross_origin_resource_policy corp = cross_origin_resource_policy::NOT_SET;
    cross_origin_embedder_policy coep = cross_origin_embedder_policy::UNSAFE_NONE;
    cross_origin_opener_policy coop = cross_origin_opener_policy::UNSAFE_NONE;
};

class security_policies {
public:
    static security_policies& instance();

    void set_config(const security_policy_config& config);
    const security_policy_config& config() const { return m_config; }

    cross_origin_resource_policy parse_corp(const std::string& header) const;
    cross_origin_embedder_policy parse_coep(const std::string& header) const;
    cross_origin_opener_policy parse_coop(const std::string& header) const;

    cross_origin_request_result check_cross_origin_request(
        const std::string& initiator_origin,
        const std::string& target_origin,
        const std::string& corp_header,
        const std::string& coep_header) const;

    bool is_same_origin(const std::string& url_a, const std::string& url_b) const;
    bool is_same_site(const std::string& url_a, const std::string& url_b) const;

    void reset();

private:
    security_policies();
    ~security_policies();
    security_policies(const security_policies&) = delete;
    security_policies& operator=(const security_policies&) = delete;

    security_policy_config m_config;
    std::string extract_domain(const std::string& url) const;
    std::string extract_origin(const std::string& url) const;
};

} // namespace hre::net
