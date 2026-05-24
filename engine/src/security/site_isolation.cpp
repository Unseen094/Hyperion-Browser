#include <hre/security/site_isolation.hpp>
#include <algorithm>
#include <sstream>

namespace hre::security {

// ---- origin ---------------------------------------------------------------

std::wstring origin::to_string() const {
    if (is_opaque()) return L"opaque";
    return scheme + L"://" + host + (port > 0 ? L":" + std::to_wstring(port) : L"");
}

std::wstring origin::to_serialized() const {
    if (is_opaque()) return L"null";
    std::wstring s = scheme + L"://" + host;
    bool is_default = (scheme == L"http" && port == 80) || (scheme == L"https" && port == 443);
    if (port > 0 && !is_default) s += L":" + std::to_wstring(port);
    return s;
}

origin origin::from_url(const std::wstring& url) {
    origin o;
    size_t scheme_end = url.find(L"://");
    if (scheme_end == std::wstring::npos) return opaque();
    o.scheme = url.substr(0, scheme_end);
    size_t host_start = scheme_end + 3;
    size_t host_end = url.find_first_of(L":/?#", host_start);
    if (host_end == std::wstring::npos) {
        o.host = url.substr(host_start);
        return o;
    }
    o.host = url.substr(host_start, host_end - host_start);
    if (url[host_end] == L':') {
        size_t port_end = url.find_first_of(L"/?#", host_end + 1);
        std::wstring port_str = url.substr(host_end + 1, port_end - host_end - 1);
        o.port = std::stoi(port_str);
    }
    return o;
}

origin origin::from_tuple(const std::wstring& scheme, const std::wstring& host, int port) {
    origin o;
    o.scheme = scheme; o.host = host; o.port = port;
    return o;
}

// ---- site -----------------------------------------------------------------

site site::from_origin(const origin& o) {
    site s;
    if (o.scheme == L"http" || o.scheme == L"https") {
        s.site_origin = origin::from_tuple(o.scheme, o.host, o.port);
    } else {
        s.site_origin = o;
    }
    return s;
}

site site::from_url(const std::wstring& url) {
    return from_origin(origin::from_url(url));
}

bool site::is_same_site(const site& other) const {
    if (site_origin.is_opaque() || other.site_origin.is_opaque()) return false;
    return site_origin.scheme == other.site_origin.scheme &&
           site_origin.host == other.site_origin.host;
}

// ---- opaque_origin_registry -----------------------------------------------

opaque_origin_registry& opaque_origin_registry::instance() {
    static opaque_origin_registry inst;
    return inst;
}

std::wstring opaque_origin_registry::create_opaque_origin() {
    std::wstring id = L"opaque_" + std::to_wstring(++m_counter);
    m_opaque_origins.insert(id);
    return id;
}

bool opaque_origin_registry::is_opaque(const std::wstring& origin_str) const {
    return m_opaque_origins.find(origin_str) != m_opaque_origins.end();
}

void opaque_origin_registry::destroy_opaque(const std::wstring& origin_str) {
    m_opaque_origins.erase(origin_str);
}

// ---- site_isolation_manager -----------------------------------------------

site_isolation_manager::site_isolation_manager() = default;
site_isolation_manager::~site_isolation_manager() = default;

void site_isolation_manager::register_origin(const origin& o, uint32_t process_id) {
    m_origin_process_map[o] = process_id;
    m_process_origins[process_id].push_back(o);
}

void site_isolation_manager::unregister_origin(const origin& o) {
    auto it = m_origin_process_map.find(o);
    if (it != m_origin_process_map.end()) {
        auto& origins = m_process_origins[it->second];
        origins.erase(std::remove(origins.begin(), origins.end(), o), origins.end());
        m_origin_process_map.erase(it);
    }
}

bool site_isolation_manager::is_origin_registered(const origin& o) const {
    return m_origin_process_map.find(o) != m_origin_process_map.end();
}

uint32_t site_isolation_manager::get_process_for_origin(const origin& o) const {
    auto it = m_origin_process_map.find(o);
    return it != m_origin_process_map.end() ? it->second : 0;
}

std::vector<origin> site_isolation_manager::get_origins_for_process(uint32_t process_id) const {
    auto it = m_process_origins.find(process_id);
    return it != m_process_origins.end() ? it->second : std::vector<origin>();
}

bool site_isolation_manager::can_access_resource(const origin& requesting_origin,
                                                   const origin& target_origin,
                                                   const std::wstring& resource_type) const {
    if (requesting_origin == target_origin) return true;

    auto it = m_require_corp.find(target_origin);
    if (it != m_require_corp.end() && it->second) {
        return false;
    }

    auto corp_it = m_embedder_policies.find(target_origin);
    if (corp_it != m_embedder_policies.end() && corp_it->second == cross_origin_embedder_policy::REQUIRE_CORP) {
        return false;
    }

    if (resource_type == L"document") {
        auto coop_it = m_opener_policies.find(requesting_origin);
        if (coop_it != m_opener_policies.end() && coop_it->second == cross_origin_opener_policy::SAME_ORIGIN) {
            return false;
        }
    }

    return true;
}

bool site_isolation_manager::can_access_window(const origin& requesting_origin,
                                                 const origin& target_origin) const {
    if (requesting_origin == target_origin) return true;
    auto coop_it = m_opener_policies.find(target_origin);
    if (coop_it != m_opener_policies.end()) {
        switch (coop_it->second) {
            case cross_origin_opener_policy::SAME_ORIGIN:
                return false;
            case cross_origin_opener_policy::SAME_ORIGIN_ALLOW_POPUPS:
                return true;
            default:
                break;
        }
    }
    return true;
}

corb_decision site_isolation_manager::check_corb(const origin& requesting_origin,
                                                   const origin& target_origin,
                                                   const std::wstring& mime_type,
                                                   const std::vector<uint8_t>& response_body) const {
    corb_decision decision;
    if (requesting_origin == target_origin) return decision;

    bool is_sniffable = (mime_type.find(L"text/html") != std::wstring::npos ||
                          mime_type.find(L"text/plain") != std::wstring::npos ||
                          mime_type.find(L"application/json") != std::wstring::npos ||
                          mime_type.find(L"text/xml") != std::wstring::npos);

    if (is_sniffable && response_body.size() > 0) {
        decision.block = true;
        decision.reason = L"CORB blocked cross-origin readable response";
    }

    if (mime_type.find(L"application/json") != std::wstring::npos ||
        mime_type.find(L"text/xml") != std::wstring::npos ||
        mime_type.find(L"application/pdf") != std::wstring::npos) {
        decision.block = true;
        decision.reason = L"CORB blocked cross-origin MIME type";
    }

    return decision;
}

cross_origin_resource_policy site_isolation_manager::parse_corp_header(const std::wstring& corp_header) const {
    if (corp_header == L"same-origin") return cross_origin_resource_policy::SAME_ORIGIN;
    if (corp_header == L"same-site") return cross_origin_resource_policy::SAME_SITE;
    if (corp_header == L"cross-origin") return cross_origin_resource_policy::CROSS_ORIGIN;
    return cross_origin_resource_policy::CROSS_ORIGIN;
}

bool site_isolation_manager::check_corp(const origin& requesting_origin,
                                         const origin& target_origin,
                                         cross_origin_resource_policy corp) const {
    switch (corp) {
        case cross_origin_resource_policy::SAME_ORIGIN:
            return requesting_origin == target_origin;
        case cross_origin_resource_policy::SAME_SITE:
            return site::from_origin(requesting_origin) == site::from_origin(target_origin);
        case cross_origin_resource_policy::CROSS_ORIGIN:
            return true;
    }
    return true;
}

void site_isolation_manager::set_embedder_policy(const origin& o, cross_origin_embedder_policy p) {
    m_embedder_policies[o] = p;
}

void site_isolation_manager::set_opener_policy(const origin& o, cross_origin_opener_policy p) {
    m_opener_policies[o] = p;
}

cross_origin_embedder_policy site_isolation_manager::get_embedder_policy(const origin& o) const {
    auto it = m_embedder_policies.find(o);
    return it != m_embedder_policies.end() ? it->second : cross_origin_embedder_policy::UNSAFE_NONE;
}

cross_origin_opener_policy site_isolation_manager::get_opener_policy(const origin& o) const {
    auto it = m_opener_policies.find(o);
    return it != m_opener_policies.end() ? it->second : cross_origin_opener_policy::UNSAFE_NONE;
}

void site_isolation_manager::set_permission(const origin& o, permission_type type, permission_status::state s) {
    m_permissions[o][type].current = s;
}

permission_status site_isolation_manager::get_permission(const origin& o, permission_type type) const {
    auto oit = m_permissions.find(o);
    if (oit == m_permissions.end()) return {};
    auto pit = oit->second.find(type);
    if (pit == oit->second.end()) return {};
    return pit->second;
}

void site_isolation_manager::set_feature_policy(const origin& o, const feature_policy_directive& directive) {
    m_feature_policies[o].push_back(directive);
}

bool site_isolation_manager::is_feature_allowed(const origin& o, const std::wstring& feature_name,
                                                  const origin& requesting_origin) const {
    auto oit = m_feature_policies.find(o);
    if (oit == m_feature_policies.end()) return true;
    for (const auto& d : oit->second) {
        if (d.feature_name == feature_name) {
            if (d.all_origins) return true;
            if (d.self_origin && o == requesting_origin) return true;
            for (const auto& allowed : d.allowlist) {
                if (allowed == requesting_origin.to_string()) return true;
            }
            return false;
        }
    }
    return true;
}

bool site_isolation_manager::is_cross_origin_isolated(const origin& o) const {
    auto coep = get_embedder_policy(o);
    auto coop = get_opener_policy(o);
    return (coep == cross_origin_embedder_policy::REQUIRE_CORP &&
            coop != cross_origin_opener_policy::UNSAFE_NONE);
}

std::wstring site_isolation_manager::get_storage_partition_key(const origin& o) const {
    if (o.is_opaque()) return L"opaque:" + o.host;
    return o.scheme + L"://" + o.host + L":" + std::to_wstring(o.port);
}

} // namespace hre::security
