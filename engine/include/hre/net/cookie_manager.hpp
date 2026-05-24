#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace hre::net {

struct cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    std::chrono::system_clock::time_point expires;
    bool secure = false;
    bool http_only = false;
};

class cookie_manager {
public:
    static cookie_manager& instance() {
        static cookie_manager inst;
        return inst;
    }

    void process_set_cookie(const std::string& domain, const std::string& set_cookie_header) {
        // Simple parser for "name=value; Domain=...; Path=...; Expires=..."
        cookie c;
        c.domain = domain;
        
        std::stringstream ss(set_cookie_header);
        std::string segment;
        bool first = true;
        
        while (std::getline(ss, segment, ';')) {
            // Trim whitespace
            while (!segment.empty() && (segment.front() == ' ' || segment.front() == '\t')) {
                segment.erase(segment.begin());
            }
            while (!segment.empty() && (segment.back() == ' ' || segment.back() == '\t')) {
                segment.pop_back();
            }
            size_t eq = segment.find('=');
            std::string key, val;
            if (eq != std::string::npos) {
                key = segment.substr(0, eq);
                val = segment.substr(eq + 1);
            } else {
                key = segment;
                val = "";
            }
            // Normalize key for comparison
            std::string lkey = key;
            std::transform(lkey.begin(), lkey.end(), lkey.begin(), ::tolower);
            if (first) {
                c.name = key;
                c.value = val;
                first = false;
            } else {
                if (lkey == "domain") {
                    c.domain = val;
                } else if (lkey == "path") {
                    c.path = val;
                } else if (lkey == "secure") {
                    c.secure = true;
                } else if (lkey == "httponly") {
                    c.http_only = true;
                } // Expires attribute ignored for simplicity
            }
        }
        
        m_cookies[domain][c.name] = c;
    }

    // Return appropriate Cookie header for a given domain. If `is_secure` is true (HTTPS), include Secure cookies; otherwise exclude them.
    std::string get_cookie_header(const std::string& domain) {
        return get_cookie_header(domain, false);
    }
    std::string get_cookie_header(const std::string& domain, bool is_secure) {
        if (!m_cookies.count(domain)) return "";
        
        std::string header = "Cookie: ";
        bool first = true;
        for (const auto& [name, c] : m_cookies[domain]) {
            // Exclude Secure cookies on insecure connections
            if (c.secure && !is_secure) continue;
            // Domain/path matching could be added here (omitted for brevity)
            if (!first) header += "; ";
            header += name + "=" + c.value;
            first = false;
        }
        // If no cookies were added, return empty string
        if (first) return "";
        return header + "\r\n";
    }

private:
    cookie_manager() = default;
    std::map<std::string, std::map<std::string, cookie>> m_cookies;
};

} // namespace hre::net
