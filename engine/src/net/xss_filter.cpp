#include "hre/net/xss_filter.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

namespace hre::net {

xss_filter::xss_filter() {
    // Default CSP: strict
    csp_directives_.emplace_back("default-src", "'self'");
    csp_directives_.emplace_back("script-src", "'self'");
    csp_directives_.emplace_back("style-src", "'self' 'unsafe-inline'");
    csp_directives_.emplace_back("img-src", "'self' data: https:");
    csp_directives_.emplace_back("connect-src", "'self'");
    csp_directives_.emplace_back("font-src", "'self'");
    csp_directives_.emplace_back("object-src", "'none'");
    csp_directives_.emplace_back("base-uri", "'self'");
    csp_directives_.emplace_back("form-action", "'self'");
    csp_directives_.emplace_back("frame-ancestors", "'self'");
}

xss_filter::~xss_filter() {}

const std::vector<std::wstring>& xss_filter::xss_patterns() {
    static const std::vector<std::wstring> patterns = {
        L"<script\\s*[^>]*>",
        L"<\\s*/\\s*script\\s*>",
        L"javascript\\s*:",
        L"onerror\\s*=",
        L"onload\\s*=",
        L"onclick\\s*=",
        L"onmouseover\\s*=",
        L"onfocus\\s*=",
        L"onblur\\s*=",
        L"onchange\\s*=",
        L"onsubmit\\s*=",
        L"onreset\\s*=",
        L"onselect\\s*=",
        L"onabort\\s*=",
        L"onkeydown\\s*=",
        L"onkeypress\\s*=",
        L"onkeyup\\s*=",
        L"ondblclick\\s*=",
        L"onmousedown\\s*=",
        L"onmousemove\\s*=",
        L"onmouseout\\s*=",
        L"onmouseup\\s*=",
        L"onresize\\s*=",
        L"onscroll\\s*=",
        L"<embed\\s*[^>]*>",
        L"<object\\s*[^>]*>",
        L"<applet\\s*[^>]*>",
        L"<iframe\\s*[^>]*>",
        L"<frame\\s*[^>]*>",
        L"<style\\s*[^>]*>.*</style>",
        L"expression\\s*\\(",
        L"-moz-binding\\s*:",
        L"vbscript\\s*:",
        L"data\\s*:\\s*text/html",
        L"document\\.cookie",
        L"document\\.write",
        L"eval\\s*\\(",
        L"setTimeout\\s*\\(",
        L"setInterval\\s*\\(",
        L"new\\s+Function\\s*\\("
    };
    return patterns;
}

bool xss_filter::match_xss_pattern(const std::wstring& input) const {
    for (const auto& pattern : xss_patterns()) {
        try {
            std::wregex re(pattern, std::wregex::icase | std::wregex::optimize);
            if (std::regex_search(input, re)) return true;
        } catch (...) {}
    }
    return false;
}

bool xss_filter::contains_xss(const std::wstring& input) const {
    if (input.empty()) return false;
    return match_xss_pattern(input);
}

std::wstring xss_filter::remove_xss_patterns(const std::wstring& input) const {
    std::wstring result = input;
    for (const auto& pattern : xss_patterns()) {
        try {
            std::wregex re(pattern, std::wregex::icase | std::wregex::optimize);
            result = std::regex_replace(result, re, L"");
        } catch (...) {}
    }
    return result;
}

std::wstring xss_filter::filter(const std::wstring& input) const {
    if (input.empty()) return input;

    switch (mode_) {
    case xss_mode::Off:
        return input;
    case xss_mode::Block:
        if (contains_xss(input)) return L"";
        return input;
    case xss_mode::Sanitize:
        return sanitize(input);
    case xss_mode::Report:
        if (contains_xss(input)) {
            // In real implementation, report to endpoint
            return L"";
        }
        return input;
    default:
        return input;
    }
}

std::wstring xss_filter::sanitize(const std::wstring& html) const {
    std::wstring result = html;

    // Replace dangerous tags with safe equivalents
    // Remove script tags completely
    result = std::regex_replace(result, std::wregex(L"<script\\b[^<]*(?:(?!<\\/script>)<[^<]*)*<\\/script>", std::wregex::icase), L"");

    // Remove event handlers
    result = std::regex_replace(result, std::wregex(L"\\s+on\\w+\\s*=\\s*\"[^\"]*\"", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"\\s+on\\w+\\s*=\\s*'[^']*'", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"\\s+on\\w+\\s*=\\s*[^\\s>]+", std::wregex::icase), L"");

    // Remove javascript: URLs
    result = std::regex_replace(result, std::wregex(L"href\\s*=\\s*\"\\s*javascript\\s*:", std::wregex::icase), L"href=\"");
    result = std::regex_replace(result, std::wregex(L"href\\s*=\\s*'\\s*javascript\\s*:", std::wregex::icase), L"href='");
    result = std::regex_replace(result, std::wregex(L"src\\s*=\\s*\"\\s*javascript\\s*:", std::wregex::icase), L"src=\"");
    result = std::regex_replace(result, std::wregex(L"src\\s*=\\s*'\\s*javascript\\s*:", std::wregex::icase), L"src='");

    // Remove dangerous elements but keep content
    result = std::regex_replace(result, std::wregex(L"<\\s*style\\b[^>]*>", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"<\\s*/\\s*style\\s*>", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"<\\s*iframe\\b[^>]*>", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"<\\s*/\\s*iframe\\s*>", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"<\\s*embed\\b[^>]*>", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"<\\s*object\\b[^>]*>", std::wregex::icase), L"");
    result = std::regex_replace(result, std::wregex(L"<\\s*/\\s*object\\s*>", std::wregex::icase), L"");

    // Encode angle brackets in text nodes (simplified)
    // Keep safe tags: a, p, div, span, h1-h6, ul, ol, li, table, tr, td, th, img, br, strong, em, b, i, u, pre, code

    return result;
}

void xss_filter::set_csp_directive(const std::string& directive, const std::string& value) {
    for (auto& [d, v] : csp_directives_) {
        if (d == directive) {
            v = value;
            return;
        }
    }
    csp_directives_.emplace_back(directive, value);
}

std::string xss_filter::generate_csp_header() const {
    std::ostringstream csp;
    for (size_t i = 0; i < csp_directives_.size(); ++i) {
        if (i > 0) csp << "; ";
        csp << csp_directives_[i].first << " " << csp_directives_[i].second;
    }
    return csp.str();
}

// ===== Sandbox =====

sandbox::sandbox() {}
sandbox::~sandbox() {}

bool sandbox::can_execute_scripts() const {
    return !enabled_ || has_flag(ALLOW_SCRIPTS);
}

bool sandbox::can_submit_forms() const {
    return !enabled_ || has_flag(ALLOW_FORMS);
}

bool sandbox::can_open_popups() const {
    return !enabled_ || has_flag(ALLOW_POPUPS);
}

bool sandbox::can_access_parent() const {
    return !enabled_ || has_flag(ALLOW_SAME_ORIGIN);
}

bool sandbox::can_navigate_top() const {
    return !enabled_ || has_flag(ALLOW_TOP_NAVIGATION);
}

bool sandbox::can_download() const {
    return !enabled_ || has_flag(ALLOW_DOWNLOADS);
}

unsigned int sandbox::parse_sandbox_attribute(const std::string& attribute_value) {
    if (attribute_value.empty()) {
        // Empty sandbox = all restrictions applied
        enabled_ = true;
        flags_ = 0;
        return flags_;
    }

    enabled_ = true;
    flags_ = 0;

    std::istringstream iss(attribute_value);
    std::string token;
    while (iss >> token) {
        if (token == "allow-forms") flags_ |= ALLOW_FORMS;
        else if (token == "allow-pointer-lock") flags_ |= ALLOW_POINTER_LOCK;
        else if (token == "allow-popups") flags_ |= ALLOW_POPUPS;
        else if (token == "allow-popups-to-escape-sandbox") flags_ |= ALLOW_POPUPS_TO_ESCAPE_SANDBOX;
        else if (token == "allow-same-origin") flags_ |= ALLOW_SAME_ORIGIN;
        else if (token == "allow-scripts") flags_ |= ALLOW_SCRIPTS;
        else if (token == "allow-top-navigation") flags_ |= ALLOW_TOP_NAVIGATION;
        else if (token == "allow-top-navigation-by-user-activation") flags_ |= ALLOW_TOP_NAVIGATION_BY_USER_ACTIVATION;
        else if (token == "allow-downloads") flags_ |= ALLOW_DOWNLOADS;
        else if (token == "allow-modals") flags_ |= ALLOW_MODALS;
        else if (token == "allow-presentation") flags_ |= ALLOW_PRESENTATION;
        else if (token == "allow-orientation-lock") flags_ |= ALLOW_ORIENTATION_LOCK;
    }

    return flags_;
}

} // namespace hre::net
