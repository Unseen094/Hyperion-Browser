#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <cstdint>

namespace hre::security {

enum class csp_directive {
    DEFAULT_SRC,
    SCRIPT_SRC,
    STYLE_SRC,
    IMG_SRC,
    CONNECT_SRC,
    MEDIA_SRC,
    FRAME_SRC,
    FONT_SRC,
    OBJECT_SRC,
    BASE_URI,
    FORM_ACTION,
    FRAME_ANCESTORS,
    PLUGIN_TYPES,
    REPORT_URI,
    MANIFEST_SRC,
    WORKER_SRC,
    PREFETCH_SRC,
    NAVIGATE_TO
};

enum class csp_source_type {
    SELF,
    NONE,
    STAR,
    HOST,
    SCHEME,
    NONCE,
    HASH,
    STRICT_DYNAMIC,
    UNSAFE_INLINE,
    UNSAFE_EVAL,
    WASM_UNSAFE_EVAL,
    REPORT_SAMPLE,
    STRICT_DIV
};

struct csp_source {
    csp_source_type type = csp_source_type::HOST;
    std::wstring value;
    std::wstring scheme;
    std::wstring host;
    int port = 0;
    std::wstring path;
    bool is_secure = false;
};

struct csp_directive_entry {
    csp_directive directive = csp_directive::DEFAULT_SRC;
    std::vector<csp_source> sources;
    bool has_unsafe_inline = false;
    bool has_unsafe_eval = false;
    bool has_strict_dynamic = false;
    bool has_nonce = false;
    std::wstring nonce;
    std::set<std::wstring> hashes;
};

struct csp_policy {
    std::map<csp_directive, csp_directive_entry> directives;
    bool report_only = false;
    std::wstring report_uri;
    std::wstring report_to;

    bool allows(const std::wstring& directive_name, const std::wstring& resource_url) const;
    bool allows_inline_script(const std::wstring& nonce = L"", const std::wstring& hash = L"") const;
    bool allows_inline_style(const std::wstring& nonce = L"", const std::wstring& hash = L"") const;
    bool allows_eval() const;
    bool allows_wasm_eval() const;
    bool allows_navigation(const std::wstring& target_url) const;
    bool allows_form_action(const std::wstring& action_url) const;
    bool allows_frame_ancestor(const std::wstring& ancestor_url) const;

    bool is_empty() const { return directives.empty(); }
    void clear() { directives.clear(); }
};

class csp_parser {
public:
    static csp_policy parse(const std::wstring& header_value);
    static std::vector<csp_policy> parse_multi(const std::vector<std::wstring>& headers);
    static std::wstring serialize(const csp_policy& policy);
    static csp_directive directive_from_string(const std::wstring& name);
    static std::wstring directive_to_string(csp_directive d);
};

class csp_enforcer {
public:
    csp_enforcer();
    ~csp_enforcer();

    // Add a policy for a given origin
    void add_policy_for_origin(const std::wstring& origin, const csp_policy& policy);
    void remove_policy_for_origin(const std::wstring& origin);
    const std::map<std::wstring, std::vector<csp_policy>>& all_policies() const { return m_policies; }

    // Check operations
    bool can_load_resource(const std::wstring& origin, const std::wstring& directive_str,
                           const std::wstring& resource_url) const;
    bool can_execute_script(const std::wstring& origin, const std::wstring& nonce = L"",
                            const std::wstring& hash = L"") const;
    bool can_execute_inline_style(const std::wstring& origin, const std::wstring& nonce = L"",
                                  const std::wstring& hash = L"") const;
    bool can_use_eval(const std::wstring& origin) const;
    bool can_navigate_to(const std::wstring& origin, const std::wstring& target_url) const;

    // Violation reporting
    struct csp_violation {
        std::wstring document_uri;
        std::wstring blocked_uri;
        std::wstring violated_directive;
        std::wstring effective_directive;
        std::wstring original_policy;
        std::wstring source_file;
        int line_number = 0;
        int column_number = 0;
        uint32_t status_code = 0;
    };

    void report_violation(const csp_violation& violation);
    void set_violation_report_handler(std::function<void(const csp_violation&)> handler) {
        m_violation_handler = handler;
    }

private:
    std::map<std::wstring, std::vector<csp_policy>> m_policies;
    std::function<void(const csp_violation&)> m_violation_handler;

    bool check_directive(const std::vector<csp_policy>& policies,
                         const std::wstring& directive_str,
                         const std::wstring& resource_url) const;
};

} // namespace hre::security
