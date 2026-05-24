#include <hre/storage/cache_api.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>

namespace hre::storage {

// ---- Cache::match/matchAll -------------------------------------------------

bool cache_api::match(const std::wstring& cache_name, const std::wstring& url,
                       std::vector<uint8_t>& data,
                       const cache_query_options& options) {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return false;

    auto& entries = cache_it->second.entries;
    std::wstring search_url = url;

    // URL normalization
    if (!search_url.empty() && search_url.back() == L'/') {
        search_url.pop_back();
    }

    for (const auto& [entry_url, entry] : entries) {
        std::wstring norm_entry = entry_url;
        if (!norm_entry.empty() && norm_entry.back() == L'/') {
            norm_entry.pop_back();
        }

        bool url_match = (norm_entry == search_url);

        if (options.ignore_search) {
            url_match = true;
        }

        if (!url_match) continue;

        if (entry.is_expired()) continue;

        if (!options.ignore_vary && !entry.matches_vary({})) continue;

        data = entry.response_data;
        return true;
    }

    return false;
}

std::vector<std::wstring> cache_api::match_all(const std::wstring& cache_name,
                                                 const std::wstring& url,
                                                 const cache_query_options& options) {
    std::vector<std::wstring> matched_urls;
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return matched_urls;

    std::wstring search_url = url;
    if (!search_url.empty() && search_url.back() == L'/') {
        search_url.pop_back();
    }

    for (const auto& [entry_url, entry] : cache_it->second.entries) {
        if (entry.is_expired()) continue;

        std::wstring norm_entry = entry_url;
        if (!norm_entry.empty() && norm_entry.back() == L'/') {
            norm_entry.pop_back();
        }

        bool match = url.empty() || (norm_entry.find(search_url) != std::wstring::npos);

        if (match) {
            matched_urls.push_back(entry_url);
        }
    }

    return matched_urls;
}

// ---- Cache::put ------------------------------------------------------------

bool cache_api::put(const std::wstring& cache_name, const std::wstring& url,
                     const std::vector<uint8_t>& data,
                     const std::map<std::wstring, std::wstring>& headers,
                     int status_code, const std::wstring& status_text) {
    auto& storage = get_or_create(cache_name);

    cache_entry_ext entry;
    entry.request_url = url;
    entry.response_data = data;
    entry.response_headers = headers;
    entry.response_status_code = status_code;
    entry.response_status_text = status_text;
    entry.stored_at = std::chrono::system_clock::now();

    auto cache_it = headers.find(L"Cache-Control");
    if (cache_it != headers.end()) {
        std::wstring cc = cache_it->second;
        auto max_age_pos = cc.find(L"max-age=");
        if (max_age_pos != std::wstring::npos) {
            int max_age = std::stoi(cc.substr(max_age_pos + 8));
            entry.expires_at = entry.stored_at + std::chrono::seconds(max_age);
        }
    }
    if (entry.expires_at < entry.stored_at) {
        entry.expires_at = entry.stored_at + std::chrono::hours(1);
    }

    auto etag_it = headers.find(L"ETag");
    if (etag_it != headers.end()) entry.etag = etag_it->second;

    auto lm_it = headers.find(L"Last-Modified");
    if (lm_it != headers.end()) entry.last_modified = lm_it->second;

    auto vary_it = headers.find(L"Vary");
    if (vary_it != headers.end()) entry.vary = vary_it->second;

    // Handle Request/Response serialization fields
    auto url_it = headers.find(L":url");
    if (url_it != headers.end()) entry.request_url = url_it->second;

    storage.entries[url] = entry;
    update_size(cache_name);
    ensure_quota(cache_name);

    return true;
}

// ---- Cache::delete ---------------------------------------------------------

bool cache_api::remove(const std::wstring& cache_name, const std::wstring& url) {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return false;

    auto& entries = cache_it->second.entries;
    auto entry_it = entries.find(url);
    if (entry_it == entries.end()) return false;

    entries.erase(entry_it);
    update_size(cache_name);
    return true;
}

void cache_api::clear(const std::wstring& cache_name) {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return;

    cache_it->second.entries.clear();
    cache_it->second.total_size = 0;
}

// ---- Cache::keys -----------------------------------------------------------

std::vector<std::wstring> cache_api::keys(const std::wstring& cache_name) const {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return {};

    std::vector<std::wstring> result;
    for (const auto& [url, _] : cache_it->second.entries) {
        result.push_back(url);
    }
    return result;
}

// ---- CacheStorage::open/has/delete/keys ------------------------------------

std::shared_ptr<cache_api> cache_manager::open(const std::wstring& cache_name) {
    auto it = m_caches.find(cache_name);
    if (it != m_caches.end()) return it->second;

    auto cache = std::make_shared<cache_api>();
    cache->open_cache(cache_name);
    m_caches[cache_name] = cache;
    return cache;
}

bool cache_manager::has(const std::wstring& cache_name) const {
    return m_caches.find(cache_name) != m_caches.end();
}

bool cache_manager::delete_cache(const std::wstring& cache_name) {
    auto it = m_caches.find(cache_name);
    if (it == m_caches.end()) return false;
    it->second->clear(cache_name);
    m_caches.erase(it);
    return true;
}

std::vector<std::wstring> cache_manager::keys() const {
    std::vector<std::wstring> names;
    for (const auto& [n, _] : m_caches) names.push_back(n);
    return names;
}

bool cache_manager::match(const std::wstring& url, std::vector<uint8_t>& data) {
    for (auto& [name, cache] : m_caches) {
        cache_query_options opts;
        if (cache->match(name, url, data, opts)) return true;
    }
    return false;
}

cache_manager& cache_manager::instance() {
    static cache_manager mgr;
    return mgr;
}

// ---- Request/Response Serialization ----------------------------------------

static std::vector<uint8_t> serialize_headers(const std::map<std::wstring, std::wstring>& headers) {
    std::vector<uint8_t> out;
    for (const auto& [key, val] : headers) {
        std::wstring line = key + L": " + val + L"\r\n";
        for (wchar_t c : line) {
            out.push_back(static_cast<uint8_t>(c & 0xFF));
        }
    }
    out.push_back(0);
    return out;
}

static std::map<std::wstring, std::wstring> deserialize_headers(const uint8_t* data, size_t len) {
    std::map<std::wstring, std::wstring> headers;
    std::wstring current;
    for (size_t i = 0; i < len; ++i) {
        if (data[i] == 0) break;
        current.push_back(static_cast<wchar_t>(data[i]));
    }

    std::wistringstream stream(current);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (line.empty() || line == L"\r") continue;
        auto colon = line.find(L':');
        if (colon == std::wstring::npos) continue;
        std::wstring key = line.substr(0, colon);
        std::wstring val = line.substr(colon + 2);
        if (!val.empty() && val.back() == L'\r') val.pop_back();
        headers[key] = val;
    }
    return headers;
}

} // namespace hre::storage
