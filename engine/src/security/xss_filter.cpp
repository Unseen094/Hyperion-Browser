#include <hre/security/xss_filter.hpp>
#include <algorithm>
#include <sstream>
#include <set>
#include <regex>

namespace hre::security {

// ---- Static data ----------------------------------------------------------

std::vector<std::wstring> html_sanitizer::s_allowed_tags;
std::vector<std::wstring> html_sanitizer::s_allowed_attributes;
std::vector<std::wstring> html_sanitizer::s_allowed_protocols;

const std::vector<std::wstring>& xss_auditor::get_script_patterns() {
    static const std::vector<std::wstring> patterns = {
        L"<script", L"<\\/script>", L"javascript:", L"onerror=",
        L"onload=", L"onclick=", L"onmouseover=", L"onfocus=",
        L"onblur=", L"onsubmit=", L"onchange=", L"<iframe",
        L"<object", L"<embed", L"<applet", L"<meta",
        L"expression(", L"vbscript:", L"data:", L"document.cookie",
        L"document.write", L"eval(", L"setTimeout(", L"setInterval(",
        L"new Function(", L"<svg", L"<math", L"<link"
    };
    return patterns;
}

const std::vector<std::wstring>& xss_auditor::get_event_handlers() {
    static const std::vector<std::wstring> handlers = {
        L"onabort", L"onblur", L"onchange", L"onclick", L"ondblclick",
        L"onerror", L"onfocus", L"onkeydown", L"onkeypress", L"onkeyup",
        L"onload", L"onmousedown", L"onmousemove", L"onmouseout",
        L"onmouseover", L"onmouseup", L"onreset", L"onresize",
        L"onscroll", L"onselect", L"onsubmit", L"onunload",
        L"onpointerdown", L"onpointermove", L"onpointerup",
        L"ontouchstart", L"ontouchend", L"ontouchmove",
        L"onanimationend", L"onanimationstart", L"ontransitionend",
        L"onwheel", L"onauxclick", L"ongotpointercapture",
        L"onlostpointercapture", L"onpointercancel", L"onpointerenter",
        L"onpointerleave", L"onpointerover", L"onpointerout",
        L"ontouchcancel", L"onbeforeinput", L"oninput", L"onselectionchange",
        L"oncopy", L"oncut", L"onpaste"
    };
    return handlers;
}

const std::vector<std::wstring>& xss_auditor::get_dangerous_tags() {
    static const std::vector<std::wstring> tags = {
        L"script", L"iframe", L"object", L"embed", L"applet",
        L"meta", L"link", L"style", L"form", L"input",
        L"button", L"select", L"textarea", L"isindex",
        L"base", L"svg", L"math"
    };
    return tags;
}

const std::vector<std::wstring>& xss_auditor::get_dangerous_attributes() {
    static const std::vector<std::wstring> attrs = {
        L"onabort", L"onblur", L"onchange", L"onclick", L"ondblclick",
        L"onerror", L"onfocus", L"onkeydown", L"onkeypress", L"onkeyup",
        L"onload", L"onmousedown", L"onmousemove", L"onmouseout",
        L"onmouseover", L"onmouseup", L"onreset", L"onresize",
        L"onscroll", L"onselect", L"onsubmit", L"onunload",
        L"style", L"href", L"src", L"action", L"formaction",
        L"srcdoc", L"data", L"codebase", L"xlink:href"
    };
    return attrs;
}

// ---- xss_auditor ----------------------------------------------------------

xss_auditor::xss_auditor() = default;
xss_auditor::~xss_auditor() = default;

xss_filter_result xss_auditor::check_script_tag(const std::wstring& html_content) const {
    xss_filter_result result;
    if (!m_enabled) return result;

    std::wstring lower = html_content;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    if (lower.find(L"<script") != std::wstring::npos) {
        result.blocked = true;
        result.reason = L"Script tag detected";
        result.original_input = html_content;
        result.triggers.push_back(L"<script");
    }

    return result;
}

xss_filter_result xss_auditor::check_event_handler(const std::wstring& html_content) const {
    xss_filter_result result;
    if (!m_enabled) return result;

    std::wstring lower = html_content;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    for (const auto& handler : get_event_handlers()) {
        std::wstring pattern = handler + L"=";
        if (lower.find(pattern) != std::wstring::npos) {
            result.blocked = true;
            result.reason = L"Event handler detected: " + handler;
            result.original_input = html_content;
            result.triggers.push_back(handler);
            break;
        }
    }

    return result;
}

xss_filter_result xss_auditor::check_url(const std::wstring& url) const {
    xss_filter_result result;
    if (!m_enabled) return result;

    std::wstring lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    if (lower.find(L"javascript:") == 0) {
        result.blocked = true;
        result.reason = L"javascript: URI detected";
        result.original_input = url;
        result.triggers.push_back(L"javascript:");
    }
    if (lower.find(L"data:") == 0 && lower.find(L"text/html") != std::wstring::npos) {
        result.blocked = true;
        result.reason = L"data:text/html URI detected";
        result.original_input = url;
        result.triggers.push_back(L"data:text/html");
    }
    if (lower.find(L"vbscript:") == 0) {
        result.blocked = true;
        result.reason = L"vbscript: URI detected";
    }

    return result;
}

xss_filter_result xss_auditor::check_inline_script(const std::wstring& script_content) const {
    xss_filter_result result;
    if (!m_enabled) return result;

    if (contains_xss_pattern(script_content)) {
        result.blocked = true;
        result.reason = L"Dangerous pattern in inline script";
        result.original_input = script_content;
    }

    return result;
}

xss_filter_result xss_auditor::check_attribute(const std::wstring& attr_name, const std::wstring& attr_value) const {
    xss_filter_result result;
    if (!m_enabled) return result;

    std::wstring lower_name = attr_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::towlower);

    for (const auto& handler : get_event_handlers()) {
        if (lower_name == handler) {
            result.blocked = true;
            result.reason = L"Event handler attribute: " + attr_name;
            result.original_input = attr_value;
            result.triggers.push_back(attr_name);
            return result;
        }
    }

    if ((lower_name == L"href" || lower_name == L"src" || lower_name == L"action" ||
         lower_name == L"formaction" || lower_name == L"xlink:href")) {
        auto url_result = check_url(attr_value);
        if (url_result.blocked) return url_result;
    }

    return result;
}

std::wstring xss_auditor::sanitize_html(const std::wstring& html_content, xss_mitigation level) const {
    if (!m_enabled || level == xss_mitigation::NONE) return html_content;
    return html_sanitizer::sanitize(html_content);
}

xss_filter_result xss_auditor::detect_reflected_xss(const std::wstring& request_url,
                                                      const std::wstring& response_body) const {
    xss_filter_result result;
    if (!m_enabled) return result;

    size_t qmark = request_url.find(L'?');
    if (qmark == std::wstring::npos) return result;

    std::wstring query = request_url.substr(qmark + 1);
    std::wstring decoded_query = query;

    for (const auto& pattern : get_script_patterns()) {
        if (decoded_query.find(pattern) != std::wstring::npos &&
            response_body.find(pattern) != std::wstring::npos) {
            result.blocked = true;
            result.reason = L"Reflected XSS detected: " + pattern;
            result.triggers.push_back(pattern);
            return result;
        }
    }

    return result;
}

void xss_auditor::add_allowed_domain(const std::wstring& domain) {
    if (std::find(m_allowed_domains.begin(), m_allowed_domains.end(), domain) == m_allowed_domains.end()) {
        m_allowed_domains.push_back(domain);
    }
}

bool xss_auditor::is_domain_allowed(const std::wstring& domain) const {
    return std::find(m_allowed_domains.begin(), m_allowed_domains.end(), domain) != m_allowed_domains.end();
}

void xss_auditor::add_allowed_script_hash(const std::wstring& hash) {
    m_allowed_hashes.insert(hash);
}

bool xss_auditor::is_script_hash_allowed(const std::wstring& hash) const {
    return m_allowed_hashes.count(hash) > 0;
}

bool xss_auditor::contains_xss_pattern(const std::wstring& input) const {
    std::wstring lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& p : get_script_patterns()) {
        if (lower.find(p) != std::wstring::npos) return true;
    }
    return false;
}

bool xss_auditor::contains_event_handler(const std::wstring& input) const {
    std::wstring lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& h : get_event_handlers()) {
        if (lower.find(h + L"=") != std::wstring::npos) return true;
    }
    return false;
}

bool xss_auditor::contains_javascript_protocol(const std::wstring& input) const {
    std::wstring lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    return lower.find(L"javascript:") == 0;
}

bool xss_auditor::contains_dangerous_html(const std::wstring& input) const {
    std::wstring lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& t : get_dangerous_tags()) {
        if (lower.find(L"<" + t) != std::wstring::npos) return true;
    }
    return contains_event_handler(input) || contains_javascript_protocol(input);
}

// ---- script_blocker -------------------------------------------------------

script_blocker::script_blocker() = default;
script_blocker::~script_blocker() = default;

void script_blocker::add_rule(const script_blocking_rule& rule) { m_rules.push_back(rule); }
void script_blocker::remove_rule(const script_blocking_rule& rule) {
    m_rules.erase(std::remove(m_rules.begin(), m_rules.end(), rule), m_rules.end());
}
void script_blocker::clear_rules() { m_rules.clear(); }

bool script_blocker::should_block_script(const std::wstring& script_url) const {
    for (const auto& domain : m_blocked_domains) {
        if (script_url.find(domain) != std::wstring::npos) return true;
    }
    for (const auto& rule : m_rules) {
        if (rule.kind == script_blocking_rule::RULE_DOMAIN && script_url.find(rule.value) != std::wstring::npos) return rule.block;
        if (rule.kind == script_blocking_rule::RULE_PATTERN && script_url.find(rule.value) != std::wstring::npos) return rule.block;
    }
    return false;
}

bool script_blocker::should_block_inline_script(const std::wstring& script_content) const {
    if (m_block_inline) return true;
    (void)script_content;
    return false;
}

bool script_blocker::should_block_event_handler(const std::wstring& handler_name) const {
    std::wstring lower = handler_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& h : xss_auditor::get_event_handlers()) {
        if (lower == h) return true;
    }
    return false;
}

bool script_blocker::should_block_eval() const { return m_block_eval; }

void script_blocker::add_trusted_script(const std::wstring& url, const std::wstring& integrity_hash) {
    m_trusted_scripts.push_back({ url, integrity_hash });
}

bool script_blocker::is_script_trusted(const std::wstring& url, const std::wstring& integrity_hash) const {
    for (const auto& ts : m_trusted_scripts) {
        if (ts.url == url) {
            if (ts.integrity_hash.empty() || ts.integrity_hash == integrity_hash) return true;
        }
    }
    return false;
}

// ---- html_sanitizer -------------------------------------------------------

std::wstring html_sanitizer::sanitize(const std::wstring& html) {
    if (html.empty()) return html;
    std::wstring result;
    size_t pos = 0;
    bool in_tag = false;

    while (pos < html.size()) {
        if (html[pos] == L'<') {
            size_t end = html.find(L'>', pos);
            if (end == std::wstring::npos) {
                result += html.substr(pos);
                break;
            }
            std::wstring tag = html.substr(pos, end - pos + 1);
            std::wstring tag_lower = tag;
            std::transform(tag_lower.begin(), tag_lower.end(), tag_lower.begin(), ::towlower);

            if (tag_lower.find(L"<script") != std::wstring::npos ||
                tag_lower.find(L"<iframe") != std::wstring::npos ||
                tag_lower.find(L"<object") != std::wstring::npos ||
                tag_lower.find(L"<embed") != std::wstring::npos ||
                tag_lower.find(L"<applet") != std::wstring::npos ||
                tag_lower.find(L"<style") != std::wstring::npos ||
                tag_lower.find(L"<link") != std::wstring::npos ||
                tag_lower.find(L"<meta") != std::wstring::npos) {
                pos = end + 1;
                continue;
            }

            result += tag;
            pos = end + 1;
        } else {
            result += html[pos];
            ++pos;
        }
    }

    return result;
}

void html_sanitizer::set_allowed_tags(const std::vector<std::wstring>& tags) { s_allowed_tags = tags; }
void html_sanitizer::set_allowed_attributes(const std::vector<std::wstring>& attrs) { s_allowed_attributes = attrs; }
void html_sanitizer::set_allowed_protocols(const std::vector<std::wstring>& protocols) { s_allowed_protocols = protocols; }

const std::vector<std::wstring>& html_sanitizer::default_allowed_tags() {
    static const std::vector<std::wstring> tags = {
        L"a", L"abbr", L"acronym", L"b", L"blockquote", L"br",
        L"caption", L"cite", L"code", L"dd", L"del", L"dfn",
        L"div", L"dl", L"dt", L"em", L"h1", L"h2", L"h3",
        L"h4", L"h5", L"h6", L"hr", L"i", L"img", L"ins",
        L"kbd", L"li", L"ol", L"p", L"pre", L"q", L"s",
        L"samp", L"small", L"span", L"strong", L"sub", L"sup",
        L"table", L"tbody", L"td", L"tfoot", L"th", L"thead",
        L"tr", L"tt", L"u", L"ul", L"var"
    };
    return tags;
}

const std::vector<std::wstring>& html_sanitizer::default_allowed_attributes() {
    static const std::vector<std::wstring> attrs = {
        L"href", L"src", L"alt", L"title", L"width", L"height",
        L"class", L"id", L"style", L"rel", L"target",
        L"dir", L"lang", L"align", L"valign",
        L"border", L"cellpadding", L"cellspacing", L"colspan",
        L"rowspan", L"headers", L"scope", L"abbr", L"datetime",
        L"cite", L"charset", L"name", L"type"
    };
    return attrs;
}

const std::vector<std::wstring>& html_sanitizer::default_allowed_protocols() {
    static const std::vector<std::wstring> protocols = {
        L"http", L"https", L"mailto", L"ftp"
    };
    return protocols;
}

} // namespace hre::security
