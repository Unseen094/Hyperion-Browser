#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <cstdint>

namespace hre::security {

// ---- Origin and Site Concepts --------------------------------------------

struct origin {
    std::wstring scheme;
    std::wstring host;
    int port = 0;

    bool operator<(const origin& o) const {
        if (scheme != o.scheme) return scheme < o.scheme;
        if (host != o.host) return host < o.host;
        return port < o.port;
    }
    bool operator==(const origin& o) const {
        return scheme == o.scheme && host == o.host && port == o.port;
    }
    bool operator!=(const origin& o) const { return !(*this == o); }

    std::wstring to_string() const;
    std::wstring to_serialized() const;

    bool is_opaque() const { return host.empty(); }
    bool is_secure() const { return scheme == L"https" || scheme == L"wss"; }

    static origin from_url(const std::wstring& url);
    static origin opaque() { return { L"", L"", 0 }; }
    static origin from_tuple(const std::wstring& scheme, const std::wstring& host, int port);
};

struct site {
    origin site_origin;

    bool operator<(const site& s) const { return site_origin < s.site_origin; }
    bool operator==(const site& s) const { return site_origin == s.site_origin; }
    bool is_same_site(const site& other) const;

    static site from_origin(const origin& o);
    static site from_url(const std::wstring& url);
    std::wstring to_string() const { return site_origin.to_string(); }
};

// ---- Cross-Origin Isolation Policies --------------------------------------

enum class cross_origin_policy {
    ALLOW,
    DENY,
    SAME_ORIGIN,
    SAME_ORIGIN_ALLOW_POPUPS,
    SAME_SITE
};

enum class cross_origin_embedder_policy {
    UNSAFE_NONE,
    REQUIRE_CORP,
    CREDENTIALLESS
};

enum class cross_origin_opener_policy {
    UNSAFE_NONE,
    SAME_ORIGIN_ALLOW_POPUPS,
    SAME_ORIGIN,
    SAME_PLUS_CREDENTIALLESS
};

// ---- Cross-Origin Resource Policy (CORP) ----------------------------------

enum class cross_origin_resource_policy {
    SAME_ORIGIN,
    SAME_SITE,
    CROSS_ORIGIN
};

// ---- Cross-Origin Read Blocking (CORB) ------------------------------------

struct corb_decision {
    bool block = false;
    std::wstring reason;
    cross_origin_resource_policy required_policy = cross_origin_resource_policy::CROSS_ORIGIN;
};

// ---- Permission/Permission Policy -----------------------------------------

enum class permission_type {
    GEOLOCATION,
    NOTIFICATIONS,
    MICROPHONE,
    CAMERA,
    MIDI,
    CLIPBOARD_READ,
    CLIPBOARD_WRITE,
    PAYMENT,
    STORAGE_ACCESS
};

struct permission_status {
    enum state { GRANTED, DENIED, PROMPT };
    state current = PROMPT;
};

// ---- Feature Policy (Permissions Policy) ----------------------------------

struct feature_policy_directive {
    std::wstring feature_name;
    std::vector<std::wstring> allowlist; // origins or * or self
    bool all_origins = false;
    bool self_origin = false;
};

// ---- Main Site Isolation Manager ------------------------------------------

class site_isolation_manager {
public:
    site_isolation_manager();
    ~site_isolation_manager();

    // Origin registration
    void register_origin(const origin& o, uint32_t process_id);
    void unregister_origin(const origin& o);
    bool is_origin_registered(const origin& o) const;

    // Process-to-origin mapping
    uint32_t get_process_for_origin(const origin& o) const;
    std::vector<origin> get_origins_for_process(uint32_t process_id) const;

    // Cross-origin access checks
    bool can_access_resource(const origin& requesting_origin,
                             const origin& target_origin,
                             const std::wstring& resource_type) const;

    bool can_access_window(const origin& requesting_origin,
                           const origin& target_origin) const;

    // CORB
    corb_decision check_corb(const origin& requesting_origin,
                             const origin& target_origin,
                             const std::wstring& mime_type,
                             const std::vector<uint8_t>& response_body) const;

    // CORP header processing
    cross_origin_resource_policy parse_corp_header(const std::wstring& corp_header) const;
    bool check_corp(const origin& requesting_origin,
                    const origin& target_origin,
                    cross_origin_resource_policy corp) const;

    // COEP / COOP
    void set_embedder_policy(const origin& o, cross_origin_embedder_policy p);
    void set_opener_policy(const origin& o, cross_origin_opener_policy p);
    cross_origin_embedder_policy get_embedder_policy(const origin& o) const;
    cross_origin_opener_policy get_opener_policy(const origin& o) const;

    // Permissions
    void set_permission(const origin& o, permission_type type, permission_status::state s);
    permission_status get_permission(const origin& o, permission_type type) const;

    // Feature policy
    void set_feature_policy(const origin& o, const feature_policy_directive& directive);
    bool is_feature_allowed(const origin& o, const std::wstring& feature_name, const origin& requesting_origin) const;

    // Cross-origin isolation status
    bool is_cross_origin_isolated(const origin& o) const;
    void set_require_corp(const origin& o, bool require) { m_require_corp[o] = require; }

    // Storage partitioning
    std::wstring get_storage_partition_key(const origin& o) const;

private:
    std::map<origin, uint32_t> m_origin_process_map;
    std::map<uint32_t, std::vector<origin>> m_process_origins;

    std::map<origin, cross_origin_embedder_policy> m_embedder_policies;
    std::map<origin, cross_origin_opener_policy> m_opener_policies;
    std::map<origin, bool> m_require_corp;

    std::map<origin, std::map<permission_type, permission_status>> m_permissions;
    std::map<origin, std::vector<feature_policy_directive>> m_feature_policies;
};

// ---- Opaque Origin Registry -----------------------------------------------

class opaque_origin_registry {
public:
    static opaque_origin_registry& instance();

    std::wstring create_opaque_origin();
    bool is_opaque(const std::wstring& origin_str) const;
    void destroy_opaque(const std::wstring& origin_str);

private:
    opaque_origin_registry() = default;
    std::set<std::wstring> m_opaque_origins;
    uint64_t m_counter = 0;
};

} // namespace hre::security
