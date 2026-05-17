#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <fstream>
#include <sstream>

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
            // Trim and split by '='
            size_t eq = segment.find('=');
            if (eq != std::string::npos) {
                std::string key = segment.substr(0, eq);
                std::string val = segment.substr(eq + 1);
                
                if (first) {
                    c.name = key;
                    c.value = val;
                    first = false;
                } else {
                    // Handle attributes like Domain, Path, etc.
                }
            }
        }
        
        m_cookies[domain][c.name] = c;
    }

    std::string get_cookie_header(const std::string& domain) {
        if (!m_cookies.count(domain)) return "";
        
        std::string header = "Cookie: ";
        bool first = true;
        for (const auto& [name, c] : m_cookies[domain]) {
            if (!first) header += "; ";
            header += name + "=" + c.value;
            first = false;
        }
        return header + "\r\n";
    }

private:
    cookie_manager() = default;
    std::map<std::string, std::map<std::string, cookie>> m_cookies;
};

} // namespace hre::net
