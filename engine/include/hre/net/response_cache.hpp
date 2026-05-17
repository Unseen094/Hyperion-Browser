#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

namespace hre::net {

struct cache_entry {
    std::vector<char> data;
    std::string content_type;
    std::chrono::system_clock::time_point cached_at;
    uint32_t max_age = 3600; // 1 hour default
};

class response_cache {
public:
    static response_cache& instance() {
        static response_cache inst;
        return inst;
    }

    void store(const std::string& url, const std::vector<char>& data, const std::string& type) {
        cache_entry entry;
        entry.data = data;
        entry.content_type = type;
        entry.cached_at = std::chrono::system_clock::now();
        m_cache[url] = entry;
    }

    bool lookup(const std::string& url, std::vector<char>& out_data, std::string& out_type) {
        if (m_cache.count(url)) {
            const auto& entry = m_cache[url];
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - entry.cached_at).count() < entry.max_age) {
                out_data = entry.data;
                out_type = entry.content_type;
                return true;
            }
        }
        return false;
    }

private:
    response_cache() = default;
    std::map<std::string, cache_entry> m_cache;
};

} // namespace hre::net
