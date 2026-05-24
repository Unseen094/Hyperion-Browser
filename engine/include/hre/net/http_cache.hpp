#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <optional>
#include <cstdint>
#include "hre/net/lru_cache.hpp"

namespace hre::net {

struct cache_control {
    uint32_t max_age = 0;
    uint32_t s_max_age = 0;
    bool no_cache = false;
    bool no_store = false;
    bool must_revalidate = false;
    bool proxy_revalidate = false;
    bool is_public = false;
    bool is_private = false;
    bool immutable = false;
    uint32_t stale_while_revalidate = 0;
    uint32_t stale_if_error = 0;

    static cache_control parse(const std::string& header_value);
};

struct cached_response {
    std::vector<char> body;
    std::map<std::string, std::string> headers;
    int status_code;
    std::chrono::system_clock::time_point stored_at;
    std::string etag;
    std::string last_modified;
    cache_control control;
    std::string url;
    std::string content_type;

    bool is_fresh() const;
    bool can_revalidate() const;
    std::chrono::seconds age() const;
    std::chrono::seconds freshness_lifetime() const;
};

class http_cache {
public:
    static http_cache& instance();

    void store(const std::string& url, const cached_response& response);
    std::optional<cached_response> lookup(const std::string& url);
    void invalidate(const std::string& url);
    void clear();

    void set_memory_budget(size_t bytes);
    size_t memory_usage() const { return m_memory_usage; }
    size_t entry_count() const;

private:
    http_cache();
    ~http_cache();
    http_cache(const http_cache&) = delete;
    http_cache& operator=(const http_cache&) = delete;

    lru_cache<std::string, cached_response> m_cache;
    size_t m_memory_budget;
    size_t m_memory_usage;

    void update_memory_usage(const std::string& url, const cached_response& resp, bool add);
};

} // namespace hre::net
