#pragma once

#include <string>
#include <vector>
#include <memory>

namespace hre::net {

enum class xss_mode {
    Off,
    Sanitize,
    Block,
    Report
};

class xss_filter {
public:
    xss_filter();
    ~xss_filter();

    void set_mode(xss_mode mode) { mode_ = mode; }
    xss_mode mode() const { return mode_; }

    // Filter/scrub HTML content for XSS
    std::wstring filter(const std::wstring& input) const;

    // Check if input contains XSS patterns
    bool contains_xss(const std::wstring& input) const;

    // Block inline scripts
    void set_block_inline_scripts(bool block) { block_inline_scripts_ = block; }

    // Sanitize by removing dangerous tags
    std::wstring sanitize(const std::wstring& html) const;

    // Content Security Policy header generation
    std::string generate_csp_header() const;

    // CSP directives
    void set_csp_directive(const std::string& directive, const std::string& value);

private:
    xss_mode mode_ = xss_mode::Sanitize;
    bool block_inline_scripts_ = true;
    std::vector<std::pair<std::string, std::string>> csp_directives_;

    bool match_xss_pattern(const std::wstring& input) const;
    std::wstring remove_xss_patterns(const std::wstring& input) const;

    // Known XSS patterns
    static const std::vector<std::wstring>& xss_patterns();
};

class sandbox {
public:
    sandbox();
    ~sandbox();

    // Sandboxing flags (parsed from iframe sandbox attribute)
    enum flag {
        ALLOW_FORMS = 1 << 0,
        ALLOW_POINTER_LOCK = 1 << 1,
        ALLOW_POPUPS = 1 << 2,
        ALLOW_POPUPS_TO_ESCAPE_SANDBOX = 1 << 3,
        ALLOW_SAME_ORIGIN = 1 << 4,
        ALLOW_SCRIPTS = 1 << 5,
        ALLOW_TOP_NAVIGATION = 1 << 6,
        ALLOW_TOP_NAVIGATION_BY_USER_ACTIVATION = 1 << 7,
        ALLOW_DOWNLOADS = 1 << 8,
        ALLOW_MODALS = 1 << 9,
        ALLOW_PRESENTATION = 1 << 10,
        ALLOW_ORIENTATION_LOCK = 1 << 11
    };

    void set_flags(unsigned int flags) { flags_ = flags; }
    unsigned int flags() const { return flags_; }
    bool has_flag(flag f) const { return (flags_ & f) != 0; }

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    bool can_execute_scripts() const;
    bool can_submit_forms() const;
    bool can_open_popups() const;
    bool can_access_parent() const;
    bool can_navigate_top() const;
    bool can_download() const;

    // Parse sandbox attribute value
    unsigned int parse_sandbox_attribute(const std::string& attribute_value);

private:
    unsigned int flags_ = 0;
    bool enabled_ = false;
};

} // namespace hre::net
