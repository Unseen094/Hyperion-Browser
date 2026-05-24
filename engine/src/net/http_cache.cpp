#include "hre/net/http_cache.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace hre::net {

cache_control cache_control::parse(const std::string& header_value) {
    cache_control cc;
    if (header_value.empty()) return cc;

    std::stringstream ss(header_value);
    std::string directive;
    while (std::getline(ss, directive, ',')) {
        directive.erase(0, directive.find_first_not_of(" "));
        directive.erase(directive.find_last_not_of(" ") + 1);

        if (directive == "no-cache") cc.no_cache = true;
        else if (directive == "no-store") cc.no_store = true;
        else if (directive == "must-revalidate") cc.must_revalidate = true;
        else if (directive == "proxy-revalidate") cc.proxy_revalidate = true;
        else if (directive == "public") cc.is_public = true;
        else if (directive == "private") cc.is_private = true;
        else if (directive == "immutable") cc.immutable = true;
        else if (directive.find("max-age=") == 0) {
            cc.max_age = static_cast<uint32_t>(std::stoul(directive.substr(8)));
        }
        else if (directive.find("s-maxage=") == 0) {
            cc.s_max_age = static_cast<uint32_t>(std::stoul(directive.substr(9)));
        }
        else if (directive.find("stale-while-revalidate=") == 0) {
            cc.stale_while_revalidate = static_cast<uint32_t>(std::stoul(directive.substr(23)));
        }
        else if (directive.find("stale-if-error=") == 0) {
            cc.stale_if_error = static_cast<uint32_t>(std::stoul(directive.substr(15)));
        }
    }
    return cc;
}

std::chrono::seconds cached_response::age() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - stored_at);
}

std::chrono::seconds cached_response::freshness_lifetime() const {
    uint32_t lifetime = control.s_max_age > 0 ? control.s_max_age : control.max_age;
    if (lifetime == 0 && status_code == 200) {
        lifetime = 3600; // default 1 hour for 200 OK
    }
    return std::chrono::seconds(lifetime);
}

bool cached_response::is_fresh() const {
    if (control.no_cache || control.no_store) return false;
    return age() < freshness_lifetime();
}

bool cached_response::can_revalidate() const {
    return !etag.empty() || !last_modified.empty();
}

http_cache::http_cache()
    : m_cache(512)
    , m_memory_budget(50 * 1024 * 1024)
    , m_memory_usage(0) {}

http_cache::~http_cache() { clear(); }

http_cache& http_cache::instance() {
    static http_cache cache;
    return cache;
}

void http_cache::store(const std::string& url, const cached_response& response) {
    if (response.control.no_store) return;

    m_cache.put(url, response);
    update_memory_usage(url, response, true);
}

std::optional<cached_response> http_cache::lookup(const std::string& url) {
    auto result = m_cache.get(url);
    if (!result.has_value()) return std::nullopt;

    if (result->is_fresh()) {
        return result;
    }
    if (!result->can_revalidate()) {
        m_cache.remove(url);
        update_memory_usage(url, *result, false);
        return std::nullopt;
    }
    return result;
}

void http_cache::invalidate(const std::string& url) {
    auto result = m_cache.get(url);
    if (result.has_value()) {
        update_memory_usage(url, *result, false);
    }
    m_cache.remove(url);
}

void http_cache::clear() {
    m_cache.clear();
    m_memory_usage = 0;
}

size_t http_cache::entry_count() const {
    return m_cache.size();
}

void http_cache::set_memory_budget(size_t bytes) {
    m_memory_budget = bytes;
}

void http_cache::update_memory_usage(const std::string& url, const cached_response& resp, bool add) {
    size_t size = url.size() + resp.body.size();
    for (const auto& [k, v] : resp.headers) {
        size += k.size() + v.size();
    }
    if (add) {
        m_memory_usage += size;
    } else {
        m_memory_usage = (size > m_memory_usage) ? 0 : m_memory_usage - size;
    }
}

} // namespace hre::net
