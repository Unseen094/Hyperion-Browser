#include <hre/security/csp.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace hre::security {

// ---- Source expression matching ----

static bool host_matches(const csp_source& src, const std::wstring& url) {
    if (src.host.empty()) return false;
    size_t scheme_end = url.find(L"://");
    if (scheme_end == std::wstring::npos) return false;
    std::wstring rest = url.substr(scheme_end + 3);
    size_t path_start = rest.find(L'/');
    std::wstring host_part = (path_start == std::wstring::npos) ? rest : rest.substr(0, path_start);
    size_t port_colon = host_part.find_last_of(L':');
    std::wstring host_only = (port_colon == std::wstring::npos) ? host_part : host_part.substr(0, port_colon);

    if (src.host.find(L'*') != std::wstring::npos) {
        size_t ast = src.host.find(L'*');
        std::wstring prefix = src.host.substr(0, ast);
        std::wstring suffix = src.host.substr(ast + 1);
        if (host_only.size() < prefix.size() + suffix.size()) return false;
        if (host_only.substr(0, prefix.size()) != prefix) return false;
        if (host_only.substr(host_only.size() - suffix.size()) != suffix) return false;
        return true;
    }
    return host_only == src.host;
}

static bool nonce_matches(const csp_source& src, const std::wstring& nonce) {
    return src.type == csp_source_type::NONCE && src.value == nonce;
}

static bool hash_matches(const csp_source& src, const std::wstring& hash) {
    return src.type == csp_source_type::HASH && src.value == hash;
}

// ---- CSP directive parsing (extended) ----

csp_directive_entry csp_parser::parse_directive_value(const std::wstring& directive_value) {
    csp_directive_entry entry;
    std::wistringstream stream(directive_value);
    std::wstring token;
    while (stream >> token) {
        csp_source src = parse_source(token);
        entry.sources.push_back(src);
        switch (src.type) {
            case csp_source_type::UNSAFE_INLINE: entry.has_unsafe_inline = true; break;
            case csp_source_type::UNSAFE_EVAL: entry.has_unsafe_eval = true; break;
            case csp_source_type::NONCE: entry.has_nonce = true; entry.nonce = src.value; break;
            case csp_source_type::HASH: entry.hashes.insert(src.value); break;
            case csp_source_type::STRICT_DYNAMIC: entry.has_strict_dynamic = true; break;
            default: break;
        }
    }
    return entry;
}

// ---- Strict-dynamic upgrade ----

void csp_policy::upgrade_strict_dynamic() {
    auto it = directives.find(csp_directive::SCRIPT_SRC);
    if (it == directives.end()) return;
    if (!it->second.has_strict_dynamic) return;

    csp_directive_entry upgraded;
    upgraded.directive = csp_directive::SCRIPT_SRC;
    upgraded.has_strict_dynamic = true;

    for (const auto& src : it->second.sources) {
        if (src.type == csp_source_type::NONCE || src.type == csp_source_type::HASH) {
            upgraded.sources.push_back(src);
            if (src.type == csp_source_type::NONCE) { upgraded.has_nonce = true; upgraded.nonce = src.value; }
            if (src.type == csp_source_type::HASH) upgraded.hashes.insert(src.value);
        }
    }

    if (upgraded.sources.empty()) return;
    directives[csp_directive::SCRIPT_SRC] = upgraded;
}

void csp_policy::upgrade_requests() {
    upgrade_strict_dynamic();
    auto it = directives.find(csp_directive::DEFAULT_SRC);
    if (it != directives.end()) {
        for (auto& [dir, entry] : directives) {
            if (dir == csp_directive::DEFAULT_SRC) continue;
            if (entry.sources.empty()) {
                entry = it->second;
            }
        }
    }
}

// ---- Violation report generation ----

std::wstring csp_enforcer::generate_violation_report(const csp_violation& violation) const {
    std::wostringstream os;
    os << L"{\n";
    os << L"  \"csp-report\": {\n";
    os << L"    \"document-uri\": \"" << violation.document_uri << L"\",\n";
    os << L"    \"blocked-uri\": \"" << violation.blocked_uri << L"\",\n";
    os << L"    \"violated-directive\": \"" << violation.violated_directive << L"\",\n";
    os << L"    \"effective-directive\": \"" << violation.effective_directive << L"\",\n";
    os << L"    \"original-policy\": \"" << violation.original_policy << L"\",\n";
    os << L"    \"disposition\": \"" << (violation.status_code == 0 ? L"enforce" : L"report") << L"\",\n";
    if (!violation.source_file.empty()) {
        os << L"    \"source-file\": \"" << violation.source_file << L"\",\n";
        os << L"    \"line-number\": " << violation.line_number << L",\n";
        os << L"    \"column-number\": " << violation.column_number << L",\n";
    }
    os << L"    \"status-code\": " << violation.status_code << L"\n";
    os << L"  }\n";
    os << L"}";
    return os.str();
}

bool csp_enforcer::has_violation(const std::wstring& origin, const std::wstring& directive_str,
                                  const std::wstring& resource_url) const {
    auto it = m_policies.find(origin);
    if (it == m_policies.end()) return false;
    return !check_directive(it->second, directive_str, resource_url);
}

void csp_enforcer::check_and_report(const std::wstring& origin, const std::wstring& directive_str,
                                     const std::wstring& resource_url) {
    if (has_violation(origin, directive_str, resource_url)) {
        csp_violation v;
        v.document_uri = origin;
        v.blocked_uri = resource_url;
        v.violated_directive = directive_str;
        v.effective_directive = directive_str;
        report_violation(v);
    }
}

} // namespace hre::security
