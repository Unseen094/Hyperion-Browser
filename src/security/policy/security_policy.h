#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <algorithm>
#include <ctime>
#include <mutex>

namespace hyperion::security::policy {

// Content Security Policy directive types
enum class CspsDirective : uint8_t {
    DEFAULT_SRC,
    SCRIPT_SRC,
    STYLE_SRC,
    IMG_SRC,
    CONNECT_SRC,
    FONT_SRC,
    OBJECT_SRC,
    MEDIA_SRC,
    FRAME_SRC,
    CHILD_SRC,
    FORM_ACTION,
    BASE_URI,
    REPORT_URI,
    SANDBOX,
    UPGRADE_INSECURE_REQUESTS,
    BLOCK_ALL_MIXED_CONTENT,
    REFLECTED_XSS_PROTECTION,
    REFERRER,
    CORS,
};

class SecurityPolicy {
public:
    // Parse and evaluate CSP
    bool evaluate_policy(const std::string& csp_header) {
        if (csp_header.empty()) return true; // Default-allowed if no CSP
        
        // Parse CSP header
        std::string csp = normalize_header(csp_header);
        auto directives = split_csp(csp);
        
        // Validate each directive
        for (const auto& [directive, source_list] : directives) {
            if (!validate_directive(directive, source_list)) {
                return false; // Policy violation detected
            }
        }
        
        // Cache policy
        last_csp_ = csp;
        cached_time_ = std::time(nullptr);
        return true;
    }
    
    // Get parsed CSP directives
    std::unordered_map<std::string, std::vector<std::string>> get_parsed_csp() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return parsed_csp_;
    }
    
    // Check if resource URI is allowed by CSP
    bool is_resource_allowed(const std::string& resource_type, const std::string& uri,
                           const std::string& document_url = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = parsed_csp_.find(resource_type);
        if (it == parsed_csp_.end()) {
            // If no directive exists, use default-src
            it = parsed_csp_.find("default-src");
            if (it == parsed_csp_.end()) {
                return true; // Default policy allows
            }
        }
        
        const auto& sources = it->second;
        if (sources.empty()) return true;
        
        if (std::find(sources.begin(), sources.end(), "*") != sources.end()) {
            return true; // Wildcard allows everything
        }
        
        if (std::find(sources.begin(), sources.end(), "self") != sources.end()) {
            if (uri.find("http://localhost") == 0 || 
                uri.find("http://127.0.0.1") == 0 ||
                uri.find("file://") == 0) {
                return true;
            }
        }
        
        // Check data: and blob: URIs
        if (std::find(sources.begin(), sources.end(), "data:") != sources.end() && 
            uri.find("data:") == 0) {
            return true;
        }
        if (std::find(sources.begin(), sources.end(), "blob:") != sources.end() && 
            uri.find("blob:") == 0) {
            return true;
        }
        
        // Check URLs
        for (const auto& source : sources) {
            if (source.find("http://") == 0 || source.find("https://") == 0) {
                if (uri.find(source) == 0) {
                    return true;
                }
            } else if (source.find("//") == 0) {
                if (check_domain_match(source.substr(2), uri)) {
                    return true;
                }
            }
        }
        
        return false; // Blocked by CSP
    }
    
    // Evaluate Cross-Origin Resource Sharing (CORS) permissions
    bool evaluate_cors(const std::string& request_origin, const std::vector<std::string>& allowed_origins) {
        if (allowed_origins.empty()) return false; // Default deny if no origin allowed
        
        // Check exact match or wildcard
        for (const auto& origin : allowed_origins) {
            if (origin == "*" || origin == request_origin) {
                return true;
            }
        }
        
        return false;
    }
    
    // Check if navigation is allowed to URL
    bool is_navigation_allowed(const std::string& target_url, 
                            const std::string& referrer = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check default-src
        auto it = parsed_csp_.find("default-src");
        if (it != parsed_csp_.end() && is_policy_blocked(it->second)) {
            return false;
        }
        
        // Check frame-src
        it = parsed_csp_.find("frame-src");
        if (it != parsed_csp_.end() && is_policy_blocked(it->second)) {
            return false;
        }
        
        return true;
    }
    
    // Secure contexts check
    bool is_secure_context(const std::string& document_url) {
        return document_url.find("https://") == 0 ||
               document_url.find("wss://") == 0 ||
               document_url.find("file://") == 0 ||
               document_url.find("localhost") != std::string::npos ||
               document_url.find("127.0.0.1") != std::string::npos;
    }
    
    // Evaluate strict transport security (HSTS)
    bool is_hsts_active(const std::string& host) {
        std::lock_guard<std::mutex> lock(mutex_);
        return hsts_hosts_.find(host) != hsts_hosts_.end();
    }
    
    // Add HSTS host
    void add_hsts_host(const std::string& host, int max_age = 31536000) {
        std::lock_guard<std::mutex> lock(mutex_);
        hsts_hosts_[host] = max_age;
    }
    
    // Mixed content blocking
    bool block_mixed_content = true;
    bool upgrade_insecure_requests = false;
    
    // XSS protection evaluation
    std::string xss_protection_mode = "sanitize"; // sanitize, filter, or block

private:
    std::string normalize_header(const std::string& header) {
        std::string normalized;
        std::regex comma_re(",");
        normalized = std::regex_replace(header, comma_re, "; ");
        
        std::regex space_re("\\s+");
        normalized = std::regex_replace(normalized, space_re, " ");
        
        return normalized;
    }
    
    std::unordered_map<std::string, std::vector<std::string>> 
    split_csp(const std::string& csp) {
        std::unordered_map<std::string, std::vector<std::string>> directives;
        
        size_t pos = 0;
        while (pos < csp.size()) {
            size_t semicolon = csp.find(';', pos);
            std::string part = (semicolon == std::string::npos) ? 
                             csp.substr(pos) : csp.substr(pos, semicolon - pos);
            
            size_t colon = part.find(':');
            if (colon != std::string::npos) {
                std::string directive = part.substr(0, colon);
                std::string sources = part.substr(colon + 1);
                
                std::transform(directive.begin(), directive.end(), directive.begin(), ::tolower);
                
                std::vector<std::string> source_list;
                size_t src_start = 0;
                while (src_start < sources.size()) {
                    size_t space = sources.find(' ', src_start);
                    std::string source = (space == std::string::npos) ? 
                                       sources.substr(src_start) : 
                                       sources.substr(src_start, space - src_start);
                    
                    if (!source.empty()) {
                        source_list.push_back(source);
                    }
                    src_start = (space == std::string::npos) ? sources.size() : space + 1;
                }
                
                directives[directive] = source_list;
                parsed_csp_[directive] = source_list; // Cache for later
            }
            
            if (semicolon == std::string::npos) break;
            pos = semicolon + 1;
        }
        
        return directives;
    }
    
    bool validate_directive(const std::string& directive, const std::vector<std::string>& sources) {
        if (block_mixed_content && directive == "block-all-mixed-content") {
            // Enforce policy if enabled
            return true;
        }
        
        return true; // Simplified validation
    }
    
    bool is_policy_blocked(const std::vector<std::string>& sources) {
        return std::find(sources.begin(), sources.end(), "'none'") != sources.end();
    }
    
    bool check_domain_match(const std::string& source_domain, const std::string& uri) {
        std::string uri_domain = extract_domain(uri);
        std::string source_pattern = source_domain;
        
        if (source_pattern.find(".") == 0) {
            // *.example.com
            std::string base_domain = source_pattern.substr(2);
            return ends_with(uri_domain, base_domain);
        }
        
        return uri_domain == source_pattern;
    }
    
    std::string extract_domain(const std::string& uri) {
        size_t protocol_end = uri.find("://");
        if (protocol_end != std::string::npos) {
            size_t host_start = protocol_end + 3;
            size_t host_end = uri.find('/', host_start);
            if (host_end != std::string::npos) {
                return uri.substr(host_start, host_end - host_start);
            }
            return uri.substr(host_start);
        }
        return {};
    }
    
    bool ends_with(const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() && 
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
    
    // Security policy state
    std::unordered_map<std::string, std::vector<std::string>> parsed_csp_;
    std::unordered_map<std::string, int> hsts_hosts_;
    std::string last_csp_;
    std::time_t cached_time_ = 0;
    bool has_active_policy_ = false;
    mutable std::mutex mutex_;
};

// XSS protection and script blocking
class XssProtection {
public:
    // Filter inline scripts and eval
    std::string sanitize_html(const std::string& html) {
        std::string sanitized;
        std::regex script_re("<script[^>]*>.*?</script\\s*>", std::regex_constants::icase);
        sanitized = std::regex_replace(html, script_re, "");
        
        std::regex event_re("on\\w+\\s*=", std::regex_constants::icase);
        sanitized = std::regex_replace(sanitized, event_re, "");
        
        std::regex eval_re("\b(eval\\s*\(|new\\s+Function\\s*\(", std::regex_constants::icase);
        sanitized = std::regex_replace(sanitized, eval_re, "noscript(");
        
        return sanitized;
    }
    
    // Block inline event handlers
    bool block_inline_event_handler(const std::string& html) {
        return html.find("onload=") != std::string::npos ||
               html.find("onclick=") != std::string::npos ||
               html.find("onmouseover=") != std::string::npos;
    }
    
    // Content sniffing protection
    bool is_dangerous_content_type(const std::string& mime_type) {
        const std::unordered_set<std::string> dangerous_types = {
            "application/x-msdownload",
            "application/x-msdos-program",
            "application/octet-stream",
            "application/x-sh",
            "application/java-archive",
            "text/html",
        };
        return dangerous_types.find(mime_type) != dangerous_types.end();
    }
};

} // namespace hyperion::security::policy