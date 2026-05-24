#include <hre/security/csp.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace hre::security {

csp_directive csp_parser::directive_from_string(const std::wstring& name) {
    if (name == L"default-src") return csp_directive::DEFAULT_SRC;
    if (name == L"script-src") return csp_directive::SCRIPT_SRC;
    if (name == L"style-src") return csp_directive::STYLE_SRC;
    if (name == L"img-src") return csp_directive::IMG_SRC;
    if (name == L"connect-src") return csp_directive::CONNECT_SRC;
    if (name == L"media-src") return csp_directive::MEDIA_SRC;
    if (name == L"frame-src") return csp_directive::FRAME_SRC;
    if (name == L"font-src") return csp_directive::FONT_SRC;
    if (name == L"object-src") return csp_directive::OBJECT_SRC;
    if (name == L"base-uri") return csp_directive::BASE_URI;
    if (name == L"form-action") return csp_directive::FORM_ACTION;
    if (name == L"frame-ancestors") return csp_directive::FRAME_ANCESTORS;
    if (name == L"plugin-types") return csp_directive::PLUGIN_TYPES;
    if (name == L"report-uri") return csp_directive::REPORT_URI;
    if (name == L"manifest-src") return csp_directive::MANIFEST_SRC;
    if (name == L"worker-src") return csp_directive::WORKER_SRC;
    if (name == L"prefetch-src") return csp_directive::PREFETCH_SRC;
    if (name == L"navigate-to") return csp_directive::NAVIGATE_TO;
    return csp_directive::DEFAULT_SRC;
}

std::wstring csp_parser::directive_to_string(csp_directive d) {
    switch (d) {
        case csp_directive::DEFAULT_SRC: return L"default-src";
        case csp_directive::SCRIPT_SRC: return L"script-src";
        case csp_directive::STYLE_SRC: return L"style-src";
        case csp_directive::IMG_SRC: return L"img-src";
        case csp_directive::CONNECT_SRC: return L"connect-src";
        case csp_directive::MEDIA_SRC: return L"media-src";
        case csp_directive::FRAME_SRC: return L"frame-src";
        case csp_directive::FONT_SRC: return L"font-src";
        case csp_directive::OBJECT_SRC: return L"object-src";
        case csp_directive::BASE_URI: return L"base-uri";
        case csp_directive::FORM_ACTION: return L"form-action";
        case csp_directive::FRAME_ANCESTORS: return L"frame-ancestors";
        case csp_directive::PLUGIN_TYPES: return L"plugin-types";
        case csp_directive::REPORT_URI: return L"report-uri";
        case csp_directive::MANIFEST_SRC: return L"manifest-src";
        case csp_directive::WORKER_SRC: return L"worker-src";
        case csp_directive::PREFETCH_SRC: return L"prefetch-src";
        case csp_directive::NAVIGATE_TO: return L"navigate-to";
    }
    return L"unknown";
}

static csp_source parse_source(const std::wstring& token) {
    csp_source src;
    if (token == L"'self'") { src.type = csp_source_type::SELF; return src; }
    if (token == L"'none'") { src.type = csp_source_type::NONE; return src; }
    if (token == L"*") { src.type = csp_source_type::STAR; return src; }
    if (token.find(L"'nonce-") == 0) {
        src.type = csp_source_type::NONCE;
        src.value = token.substr(7, token.size() - 8);
        return src;
    }
    if (token.find(L"'sha") == 0) {
        src.type = csp_source_type::HASH;
        src.value = token;
        return src;
    }
    if (token == L"'strict-dynamic'") { src.type = csp_source_type::STRICT_DYNAMIC; return src; }
    if (token == L"'unsafe-inline'") { src.type = csp_source_type::UNSAFE_INLINE; return src; }
    if (token == L"'unsafe-eval'") { src.type = csp_source_type::UNSAFE_EVAL; return src; }
    if (token == L"'wasm-unsafe-eval'") { src.type = csp_source_type::WASM_UNSAFE_EVAL; return src; }
    if (token == L"'report-sample'") { src.type = csp_source_type::REPORT_SAMPLE; return src; }
    if (token == L"'strict-dv'") { src.type = csp_source_type::STRICT_DIV; return src; }

    size_t colon_slash = token.find(L"://");
    if (colon_slash != std::wstring::npos) {
        src.scheme = token.substr(0, colon_slash);
        std::wstring rest = token.substr(colon_slash + 3);
        size_t path_start = rest.find(L'/');
        std::wstring host_part = (path_start == std::wstring::npos) ? rest : rest.substr(0, path_start);
        size_t port_colon = host_part.find_last_of(L':');
        if (port_colon != std::wstring::npos) {
            src.host = host_part.substr(0, port_colon);
            src.port = std::stoi(host_part.substr(port_colon + 1));
        } else {
            src.host = host_part;
        }
        if (path_start != std::wstring::npos) src.path = rest.substr(path_start);
    } else if (token.find(L"http://") == 0 || token.find(L"https://") == 0) {
        src.type = csp_source_type::HOST;
        src.value = token;
    } else {
        src.type = csp_source_type::SCHEME;
        src.value = token;
    }

    return src;
}

csp_policy csp_parser::parse(const std::wstring& header_value) {
    csp_policy policy;
    std::wistringstream stream(header_value);
    std::wstring token;
    csp_directive current_dir = csp_directive::DEFAULT_SRC;

    while (stream >> token) {
        if (token.empty()) continue;

        if (token.back() == L';') {
            token.pop_back();
            if (token.empty()) continue;
        }

        if (token[0] != L'\'' && token.find(L'-') != std::wstring::npos &&
            token != L"*" && token.find(L"://") == std::wstring::npos &&
            token.find(L'/') == std::wstring::npos) {
            current_dir = directive_from_string(token);
            policy.directives[current_dir] = {};
            continue;
        }

        csp_source src = parse_source(token);
        auto& entry = policy.directives[current_dir];
        entry.directive = current_dir;
        entry.sources.push_back(src);

        switch (src.type) {
            case csp_source_type::UNSAFE_INLINE:
                if (current_dir == csp_directive::SCRIPT_SRC) entry.has_unsafe_inline = true;
                if (current_dir == csp_directive::STYLE_SRC) entry.has_unsafe_inline = true;
                break;
            case csp_source_type::UNSAFE_EVAL:
                if (current_dir == csp_directive::SCRIPT_SRC) entry.has_unsafe_eval = true;
                break;
            case csp_source_type::NONCE:
                entry.has_nonce = true;
                entry.nonce = src.value;
                break;
            case csp_source_type::HASH:
                entry.hashes.insert(src.value);
                break;
            case csp_source_type::STRICT_DYNAMIC:
                entry.has_strict_dynamic = true;
                break;
            default:
                break;
        }
    }

    return policy;
}

std::vector<csp_policy> csp_parser::parse_multi(const std::vector<std::wstring>& headers) {
    std::vector<csp_policy> policies;
    for (const auto& h : headers) {
        policies.push_back(parse(h));
    }
    return policies;
}

std::wstring csp_parser::serialize(const csp_policy& policy) {
    std::wstring result;
    for (const auto& [dir, entry] : policy.directives) {
        result += directive_to_string(dir) + L" ";
        for (const auto& src : entry.sources) {
            switch (src.type) {
                case csp_source_type::SELF: result += L"'self' "; break;
                case csp_source_type::NONE: result += L"'none' "; break;
                case csp_source_type::STAR: result += L"* "; break;
                case csp_source_type::NONCE: result += L"'nonce-" + src.value + L"' "; break;
                case csp_source_type::HASH: result += src.value + L" "; break;
                case csp_source_type::UNSAFE_INLINE: result += L"'unsafe-inline' "; break;
                case csp_source_type::UNSAFE_EVAL: result += L"'unsafe-eval' "; break;
                case csp_source_type::STRICT_DYNAMIC: result += L"'strict-dynamic' "; break;
                case csp_source_type::WASM_UNSAFE_EVAL: result += L"'wasm-unsafe-eval' "; break;
                default: result += src.value + L" "; break;
            }
        }
        result += L"; ";
    }
    return result;
}

// ---- CSP Policy query methods --------------------------------------------

bool csp_policy::allows(const std::wstring& directive_name, const std::wstring& resource_url) const {
    csp_directive dir = csp_parser::directive_from_string(directive_name);
    auto it = directives.find(dir);
    if (it == directives.end()) it = directives.find(csp_directive::DEFAULT_SRC);
    if (it == directives.end()) return true;

    for (const auto& src : it->second.sources) {
        switch (src.type) {
            case csp_source_type::STAR: return true;
            case csp_source_type::SELF: return true;
            case csp_source_type::HOST:
                if (resource_url.find(src.value) != std::wstring::npos) return true;
                break;
            case csp_source_type::SCHEME:
                if (resource_url.find(src.scheme) == 0) return true;
                break;
            default:
                break;
        }
    }
    return false;
}

bool csp_policy::allows_inline_script(const std::wstring& nonce, const std::wstring& hash) const {
    auto it = directives.find(csp_directive::SCRIPT_SRC);
    if (it == directives.end()) it = directives.find(csp_directive::DEFAULT_SRC);
    if (it == directives.end()) return true;

    if (it->second.has_nonce && !nonce.empty()) {
        return it->second.nonce == nonce;
    }
    if (!hash.empty() && it->second.hashes.count(hash)) return true;
    if (it->second.has_strict_dynamic) return !nonce.empty() || !hash.empty();
    return it->second.has_unsafe_inline;
}

bool csp_policy::allows_inline_style(const std::wstring& nonce, const std::wstring& hash) const {
    auto it = directives.find(csp_directive::STYLE_SRC);
    if (it == directives.end()) it = directives.find(csp_directive::DEFAULT_SRC);
    if (it == directives.end()) return true;
    if (it->second.has_nonce && !nonce.empty()) return it->second.nonce == nonce;
    if (!hash.empty() && it->second.hashes.count(hash)) return true;
    return it->second.has_unsafe_inline;
}

bool csp_policy::allows_eval() const {
    auto it = directives.find(csp_directive::SCRIPT_SRC);
    if (it == directives.end()) it = directives.find(csp_directive::DEFAULT_SRC);
    if (it == directives.end()) return true;
    return it->second.has_unsafe_eval;
}

bool csp_policy::allows_wasm_eval() const {
    auto it = directives.find(csp_directive::SCRIPT_SRC);
    if (it == directives.end()) it = directives.find(csp_directive::DEFAULT_SRC);
    if (it == directives.end()) return true;
    return it->second.has_unsafe_eval;
}

bool csp_policy::allows_navigation(const std::wstring& target_url) const {
    return allows(L"navigate-to", target_url);
}

bool csp_policy::allows_form_action(const std::wstring& action_url) const {
    auto it = directives.find(csp_directive::FORM_ACTION);
    if (it == directives.end()) return true;
    return allows(L"form-action", action_url);
}

bool csp_policy::allows_frame_ancestor(const std::wstring& ancestor_url) const {
    auto it = directives.find(csp_directive::FRAME_ANCESTORS);
    if (it == directives.end()) return true;
    return allows(L"frame-ancestors", ancestor_url);
}

// ---- CSP Enforcer ---------------------------------------------------------

csp_enforcer::csp_enforcer() = default;
csp_enforcer::~csp_enforcer() = default;

void csp_enforcer::add_policy_for_origin(const std::wstring& origin, const csp_policy& policy) {
    m_policies[origin].push_back(policy);
}

void csp_enforcer::remove_policy_for_origin(const std::wstring& origin) {
    m_policies.erase(origin);
}

bool csp_enforcer::can_load_resource(const std::wstring& origin, const std::wstring& directive_str,
                                      const std::wstring& resource_url) const {
    auto it = m_policies.find(origin);
    if (it == m_policies.end()) return true;
    return check_directive(it->second, directive_str, resource_url);
}

bool csp_enforcer::can_execute_script(const std::wstring& origin, const std::wstring& nonce,
                                       const std::wstring& hash) const {
    auto it = m_policies.find(origin);
    if (it == m_policies.end()) return true;
    for (const auto& p : it->second) {
        if (!p.allows_inline_script(nonce, hash)) return false;
    }
    return true;
}

bool csp_enforcer::can_execute_inline_style(const std::wstring& origin, const std::wstring& nonce,
                                             const std::wstring& hash) const {
    auto it = m_policies.find(origin);
    if (it == m_policies.end()) return true;
    for (const auto& p : it->second) {
        if (!p.allows_inline_style(nonce, hash)) return false;
    }
    return true;
}

bool csp_enforcer::can_use_eval(const std::wstring& origin) const {
    auto it = m_policies.find(origin);
    if (it == m_policies.end()) return true;
    for (const auto& p : it->second) {
        if (!p.allows_eval()) return false;
    }
    return true;
}

bool csp_enforcer::can_navigate_to(const std::wstring& origin, const std::wstring& target_url) const {
    auto it = m_policies.find(origin);
    if (it == m_policies.end()) return true;
    for (const auto& p : it->second) {
        if (!p.allows_navigation(target_url)) return false;
    }
    return true;
}

void csp_enforcer::report_violation(const csp_violation& violation) {
    if (m_violation_handler) m_violation_handler(violation);
}

bool csp_enforcer::check_directive(const std::vector<csp_policy>& policies,
                                    const std::wstring& directive_str,
                                    const std::wstring& resource_url) const {
    for (const auto& p : policies) {
        if (!p.allows(directive_str, resource_url)) return false;
    }
    return true;
}

} // namespace hre::security
