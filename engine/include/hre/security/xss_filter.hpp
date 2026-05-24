#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <cstdint>
#include <regex>

namespace hre::security {

// ---- XSS Filter / Script Blocking ----------------------------------------

enum class xss_mitigation {
    NONE,
    BLOCK,
    SANITIZE,
    REPORT
};

struct xss_filter_result {
    bool blocked = false;
    bool sanitized = false;
    bool reported = false;
    std::wstring reason;
    std::wstring original_input;
    std::wstring sanitized_output;
    std::vector<std::wstring> triggers;
};

// ---- XSS Auditor ----------------------------------------------------------

class xss_auditor {
public:
    xss_auditor();
    ~xss_auditor();

    // Main check methods
    xss_filter_result check_script_tag(const std::wstring& html_content) const;
    xss_filter_result check_event_handler(const std::wstring& html_content) const;
    xss_filter_result check_url(const std::wstring& url) const;
    xss_filter_result check_inline_script(const std::wstring& script_content) const;
    xss_filter_result check_attribute(const std::wstring& attr_name, const std::wstring& attr_value) const;

    // Comprehensive HTML sanitization
    std::wstring sanitize_html(const std::wstring& html_content, xss_mitigation level = xss_mitigation::SANITIZE) const;

    // Reflected XSS detection
    xss_filter_result detect_reflected_xss(const std::wstring& request_url,
                                            const std::wstring& response_body) const;

    // Config
    void set_mitigation(xss_mitigation m) { m_mitigation = m; }
    xss_mitigation mitigation() const { return m_mitigation; }

    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

    using xss_violation_callback = std::function<void(const xss_filter_result&)>;
    void set_violation_callback(xss_violation_callback cb) { m_callback = cb; }

    // Allowlist
    void add_allowed_domain(const std::wstring& domain);
    bool is_domain_allowed(const std::wstring& domain) const;

    void add_allowed_script_hash(const std::wstring& hash);
    bool is_script_hash_allowed(const std::wstring& hash) const;

    static const std::vector<std::wstring>& get_event_handlers();

private:
    bool m_enabled = true;
    xss_mitigation m_mitigation = xss_mitigation::SANITIZE;
    xss_violation_callback m_callback;

    std::vector<std::wstring> m_allowed_domains;
    std::set<std::wstring> m_allowed_hashes;

    // Pattern matching
    bool contains_xss_pattern(const std::wstring& input) const;
    bool contains_event_handler(const std::wstring& input) const;
    bool contains_javascript_protocol(const std::wstring& input) const;
    bool contains_dangerous_html(const std::wstring& input) const;

    // Known XSS patterns
    static const std::vector<std::wstring>& get_script_patterns();
    static const std::vector<std::wstring>& get_dangerous_tags();
    static const std::vector<std::wstring>& get_dangerous_attributes();
};

// ---- Script Blocking Rules ------------------------------------------------

struct script_blocking_rule {
    enum rule_type { RULE_DOMAIN, RULE_PATTERN, RULE_INLINE, RULE_EVENT_HANDLER, RULE_DYNAMIC_EVAL };
    rule_type kind = RULE_DOMAIN;
    std::wstring value;
    bool block = true;

    bool operator==(const script_blocking_rule& other) const {
        return kind == other.kind && value == other.value && block == other.block;
    }
};

class script_blocker {
public:
    script_blocker();
    ~script_blocker();

    void add_rule(const script_blocking_rule& rule);
    void remove_rule(const script_blocking_rule& rule);
    void clear_rules();

    bool should_block_script(const std::wstring& script_url) const;
    bool should_block_inline_script(const std::wstring& script_content) const;
    bool should_block_event_handler(const std::wstring& handler_name) const;
    bool should_block_eval() const;

    void set_block_inline_scripts(bool block) { m_block_inline = block; }
    void set_block_eval(bool block) { m_block_eval = block; }
    void set_block_domains(const std::vector<std::wstring>& domains) { m_blocked_domains = domains; }

    // Trusted script sources (CSR allowlist)
    void add_trusted_script(const std::wstring& url, const std::wstring& integrity_hash = L"");
    bool is_script_trusted(const std::wstring& url, const std::wstring& integrity_hash = L"") const;

private:
    bool m_block_inline = false;
    bool m_block_eval = false;
    std::vector<std::wstring> m_blocked_domains;
    std::vector<script_blocking_rule> m_rules;

    struct trusted_script {
        std::wstring url;
        std::wstring integrity_hash;
    };
    std::vector<trusted_script> m_trusted_scripts;
};

// ---- Content Sanitization -------------------------------------------------

class html_sanitizer {
public:
    static std::wstring sanitize(const std::wstring& html);

    // Allow/deny lists for tags and attributes
    static void set_allowed_tags(const std::vector<std::wstring>& tags);
    static void set_allowed_attributes(const std::vector<std::wstring>& attrs);
    static void set_allowed_protocols(const std::vector<std::wstring>& protocols);

    static const std::vector<std::wstring>& default_allowed_tags();
    static const std::vector<std::wstring>& default_allowed_attributes();
    static const std::vector<std::wstring>& default_allowed_protocols();

private:
    static std::vector<std::wstring> s_allowed_tags;
    static std::vector<std::wstring> s_allowed_attributes;
    static std::vector<std::wstring> s_allowed_protocols;

    static bool is_tag_allowed(const std::wstring& tag);
    static bool is_attribute_allowed(const std::wstring& tag, const std::wstring& attr);
    static bool is_protocol_allowed(const std::wstring& protocol);
    static std::wstring sanitize_attribute(const std::wstring& name, const std::wstring& value);
    static std::wstring strip_tags(const std::wstring& html);
};

} // namespace hre::security
