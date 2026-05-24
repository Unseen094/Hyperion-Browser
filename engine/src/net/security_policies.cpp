#include "hre/net/security_policies.hpp"
#include <algorithm>
#include <sstream>

namespace hre::net {

security_policies::security_policies() {}
security_policies::~security_policies() {}

security_policies& security_policies::instance() {
    static security_policies policies;
    return policies;
}

void security_policies::set_config(const security_policy_config& config) {
    m_config = config;
}

cross_origin_resource_policy security_policies::parse_corp(const std::string& header) const {
    std::string value = header;
    value.erase(0, value.find_first_not_of(" "));
    value.erase(value.find_last_not_of(" ") + 1);
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "same-site") return cross_origin_resource_policy::SAME_SITE;
    if (value == "same-origin") return cross_origin_resource_policy::SAME_ORIGIN;
    if (value == "cross-origin") return cross_origin_resource_policy::CROSS_ORIGIN;
    return cross_origin_resource_policy::NOT_SET;
}

cross_origin_embedder_policy security_policies::parse_coep(const std::string& header) const {
    std::string value = header;
    value.erase(0, value.find_first_not_of(" "));
    value.erase(value.find_last_not_of(" ") + 1);
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "require-corp") return cross_origin_embedder_policy::REQUIRE_CORP;
    if (value == "credentialless") return cross_origin_embedder_policy::CREDENTIALLESS;
    return cross_origin_embedder_policy::UNSAFE_NONE;
}

cross_origin_opener_policy security_policies::parse_coop(const std::string& header) const {
    std::string value = header;
    value.erase(0, value.find_first_not_of(" "));
    value.erase(value.find_last_not_of(" ") + 1);
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "same-origin") return cross_origin_opener_policy::SAME_ORIGIN;
    if (value == "same-origin-allow-popups") return cross_origin_opener_policy::SAME_ORIGIN_ALLOW_POPUPS;
    if (value == "restricted-properties") return cross_origin_opener_policy::RESTRICTED_PROPERTIES;
    return cross_origin_opener_policy::UNSAFE_NONE;
}

cross_origin_request_result security_policies::check_cross_origin_request(
    const std::string& initiator_origin,
    const std::string& target_origin,
    const std::string& corp_header,
    const std::string& coep_header) const {

    if (is_same_origin(initiator_origin, target_origin)) {
        return cross_origin_request_result::ALLOWED;
    }

    auto target_corp = parse_corp(corp_header);
    auto embedder = parse_coep(coep_header);

    if (target_corp == cross_origin_resource_policy::SAME_ORIGIN) {
        return cross_origin_request_result::BLOCKED_BY_CORP;
    }

    if (target_corp == cross_origin_resource_policy::SAME_SITE) {
        if (!is_same_site(initiator_origin, target_origin)) {
            return cross_origin_request_result::BLOCKED_BY_CORP;
        }
    }

    if (embedder == cross_origin_embedder_policy::REQUIRE_CORP) {
        if (target_corp == cross_origin_resource_policy::NOT_SET ||
            target_corp == cross_origin_resource_policy::CROSS_ORIGIN) {
            return cross_origin_request_result::BLOCKED_BY_COEP;
        }
    }

    if (m_config.coop == cross_origin_opener_policy::SAME_ORIGIN) {
        if (!is_same_origin(initiator_origin, target_origin)) {
            return cross_origin_request_result::BLOCKED_BY_COOP;
        }
    }

    return cross_origin_request_result::ALLOWED;
}

bool security_policies::is_same_origin(const std::string& url_a, const std::string& url_b) const {
    return extract_origin(url_a) == extract_origin(url_b);
}

bool security_policies::is_same_site(const std::string& url_a, const std::string& url_b) const {
    return extract_domain(url_a) == extract_domain(url_b);
}

std::string security_policies::extract_domain(const std::string& url) const {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;

    size_t end = url.find(':', start);
    if (end == std::string::npos) end = url.find('/', start);
    if (end == std::string::npos) end = url.size();

    std::string host = url.substr(start, end - start);
    size_t dot = host.find('.');
    if (dot != std::string::npos) {
        host = host.substr(dot + 1);
        dot = host.find('.');
        if (dot != std::string::npos) {
            host = host.substr(0, dot + 1);
        }
    }
    return host;
}

std::string security_policies::extract_origin(const std::string& url) const {
    size_t start = url.find("://");
    if (start == std::string::npos) return "";

    size_t end = url.find('/', start + 3);
    if (end == std::string::npos) return url;

    return url.substr(0, end);
}

void security_policies::reset() {
    m_config = security_policy_config{};
}

// CSP (Content Security Policy) implementation
class content_security_policy {
public:
    content_security_policy() = default;

    struct csp_directive {
        std::string name;
        std::vector<std::string> sources;
        bool has_none = false;
        bool has_self = false;
        bool has_unsafe_inline = false;
        bool has_unsafe_eval = false;
        bool has_strict_dynamic = false;
        std::vector<std::string> hashes;
        std::vector<std::string> nonces;
        std::vector<std::string> report_uris;
    };

    void parse_policy(const std::string& policy);
    bool allows_resource(const std::string& directive_name, const std::string& url,
                          const std::string& nonce = "") const;
    bool allows_inline_script(const std::string& nonce = "", const std::string& hash = "") const;
    bool allows_inline_style(const std::string& nonce = "", const std::string& hash = "") const;
    bool allows_eval() const;
    bool has_directive(const std::string& name) const;
    const csp_directive* get_directive(const std::string& name) const;

    void set_report_only(bool v) { report_only_ = v; }
    bool is_report_only() const { return report_only_; }

    std::string to_string() const;

private:
    std::vector<csp_directive> directives_;
    bool report_only_ = false;

    csp_directive parse_directive_value(const std::string& value) const;
    bool match_source(const csp_directive& dir, const std::string& url) const;
    bool match_scheme(const std::string& scheme, const std::string& source) const;
    bool match_host(const std::string& host, const std::string& source) const;
    std::string extract_scheme(const std::string& url) const;
    std::string extract_host(const std::string& url) const;
};

void content_security_policy::parse_policy(const std::string& policy) {
    directives_.clear();
    std::istringstream stream(policy);
    std::string segment;

    while (std::getline(stream, segment, ';')) {
        segment.erase(0, segment.find_first_not_of(" \t\n\r"));
        segment.erase(segment.find_last_not_of(" \t\n\r") + 1);
        if (segment.empty()) continue;

        size_t space = segment.find(' ');
        std::string name = segment.substr(0, space);
        std::string value = (space != std::string::npos) ? segment.substr(space + 1) : "";

        csp_directive directive = parse_directive_value(value);
        directive.name = name;
        directives_.push_back(directive);
    }
}

content_security_policy::csp_directive content_security_policy::parse_directive_value(const std::string& value) const {
    csp_directive dir;
    std::istringstream stream(value);
    std::string token;

    while (stream >> token) {
        if (token == "'none'") dir.has_none = true;
        else if (token == "'self'") dir.has_self = true;
        else if (token == "'unsafe-inline'") dir.has_unsafe_inline = true;
        else if (token == "'unsafe-eval'") dir.has_unsafe_eval = true;
        else if (token == "'strict-dynamic'") dir.has_strict_dynamic = true;
        else if (token.find("'nonce-") == 0) {
            dir.nonces.push_back(token.substr(7, token.size() - 8));
        } else if (token.find("'sha") == 0) {
            dir.hashes.push_back(token);
        } else if (token.find("report-uri") == 0) {
        } else if (token.find("http") == 0 || token.find("https") == 0 || token.find("data:") == 0 ||
                   token.find("blob:") == 0 || token.find("filesystem:") == 0) {
            dir.sources.push_back(token);
        } else if (token == "*") {
            dir.sources.push_back("*");
        } else if (token.find('.') != std::string::npos || token.find("://") != std::string::npos) {
            dir.sources.push_back(token);
        } else if (token.find('/') == 0) {
        }
    }

    return dir;
}

bool content_security_policy::allows_resource(const std::string& directive_name, const std::string& url,
                                               const std::string& nonce) const {
    for (const auto& dir : directives_) {
        if (dir.name == directive_name || dir.name == "default-src") {
            if (dir.has_none) return false;
            if (dir.has_self) {
                return true;
            }
            if (!nonce.empty() && !dir.nonces.empty()) {
                if (std::find(dir.nonces.begin(), dir.nonces.end(), nonce) != dir.nonces.end()) return true;
            }
            if (match_source(dir, url)) return true;
        }
    }
    return true;
}

bool content_security_policy::allows_inline_script(const std::string& nonce, const std::string& hash) const {
    for (const auto& dir : directives_) {
        if (dir.name == "script-src" || dir.name == "default-src") {
            if (dir.has_strict_dynamic) return true;
            if (!nonce.empty() && std::find(dir.nonces.begin(), dir.nonces.end(), nonce) != dir.nonces.end()) return true;
            if (!hash.empty() && std::find(dir.hashes.begin(), dir.hashes.end(), hash) != dir.hashes.end()) return true;
            if (dir.has_unsafe_inline) return true;
            return false;
        }
    }
    return true;
}

bool content_security_policy::allows_inline_style(const std::string& nonce, const std::string& hash) const {
    for (const auto& dir : directives_) {
        if (dir.name == "style-src" || dir.name == "default-src") {
            if (!nonce.empty() && std::find(dir.nonces.begin(), dir.nonces.end(), nonce) != dir.nonces.end()) return true;
            if (dir.has_unsafe_inline) return true;
            return false;
        }
    }
    return true;
}

bool content_security_policy::allows_eval() const {
    for (const auto& dir : directives_) {
        if (dir.name == "script-src" || dir.name == "default-src") {
            if (dir.has_unsafe_eval) return true;
            if (dir.has_strict_dynamic) return false;
            return false;
        }
    }
    return true;
}

bool content_security_policy::has_directive(const std::string& name) const {
    for (const auto& dir : directives_) {
        if (dir.name == name) return true;
    }
    return false;
}

const content_security_policy::csp_directive* content_security_policy::get_directive(const std::string& name) const {
    for (const auto& dir : directives_) {
        if (dir.name == name) return &dir;
    }
    return nullptr;
}

bool content_security_policy::match_source(const csp_directive& dir, const std::string& url) const {
    for (const auto& src : dir.sources) {
        if (src == "*") return true;
        if (src == url) return true;
        if (match_scheme(extract_scheme(url), src)) return true;
        if (match_host(extract_host(url), src)) return true;
    }
    if (dir.has_self) return true;
    return false;
}

bool content_security_policy::match_scheme(const std::string& scheme, const std::string& source) const {
    return scheme + ":" == source || scheme + "://" == source;
}

bool content_security_policy::match_host(const std::string& host, const std::string& source) const {
    if (source.find("://") == std::string::npos && source.find(".") != std::string::npos) {
        if (host == source) return true;
        if (source[0] == '*' && source[1] == '.') {
            std::string wildcard = source.substr(1);
            if (host.size() >= wildcard.size() &&
                host.substr(host.size() - wildcard.size()) == wildcard) return true;
        }
    }
    return false;
}

std::string content_security_policy::extract_scheme(const std::string& url) const {
    size_t pos = url.find("://");
    if (pos == std::string::npos) return "";
    return url.substr(0, pos);
}

std::string content_security_policy::extract_host(const std::string& url) const {
    size_t start = url.find("://");
    if (start == std::string::npos) return url;
    start += 3;
    size_t end = url.find('/', start);
    if (end == std::string::npos) end = url.find('?', start);
    if (end == std::string::npos) return url.substr(start);
    return url.substr(start, end - start);
}

std::string content_security_policy::to_string() const {
    std::string result;
    for (const auto& dir : directives_) {
        if (!result.empty()) result += "; ";
        result += dir.name;
        if (!dir.sources.empty() || dir.has_none || dir.has_self ||
            dir.has_unsafe_inline || dir.has_unsafe_eval || dir.has_strict_dynamic ||
            !dir.nonces.empty() || !dir.hashes.empty()) {
            result += " ";
            if (dir.has_none) result += "'none' ";
            if (dir.has_self) result += "'self' ";
            if (dir.has_unsafe_inline) result += "'unsafe-inline' ";
            if (dir.has_unsafe_eval) result += "'unsafe-eval' ";
            if (dir.has_strict_dynamic) result += "'strict-dynamic' ";
            for (const auto& n : dir.nonces) result += "'nonce-" + n + "' ";
            for (const auto& h : dir.hashes) result += h + " ";
            for (const auto& s : dir.sources) result += s + " ";
            if (!result.empty() && result.back() == ' ') result.pop_back();
        }
    }
    return result;
}

} // namespace hre::net
