#include <hre/storage/cache_api.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>

namespace hre::storage {

// ---- cache_entry_ext ------------------------------------------------------

bool cache_entry_ext::is_expired() const {
    return std::chrono::system_clock::now() > expires_at;
}

bool cache_entry_ext::matches_vary(const std::map<std::wstring, std::wstring>& request_headers) const {
    if (vary.empty()) return true;

    std::wistringstream vary_stream(vary);
    std::wstring header_name;
    while (std::getline(vary_stream, header_name, L',')) {
        while (!header_name.empty() && header_name[0] == L' ') header_name.erase(0, 1);
        auto it = response_headers.find(header_name);
        auto req_it = request_headers.find(header_name);
        if (it == response_headers.end() && req_it == request_headers.end()) continue;
        if (it == response_headers.end() || req_it == request_headers.end()) return false;
        if (it->second != req_it->second) return false;
    }
    return true;
}

// ---- cache_api ------------------------------------------------------------

cache_api::cache_api() = default;
cache_api::~cache_api() = default;

bool cache_api::open_cache(const std::wstring& cache_name) {
    if (m_caches.find(cache_name) != m_caches.end()) return true;
    m_caches[cache_name] = {};
    return true;
}

bool cache_api::delete_cache(const std::wstring& cache_name) {
    return m_caches.erase(cache_name) > 0;
}

bool cache_api::has_cache(const std::wstring& cache_name) const {
    return m_caches.find(cache_name) != m_caches.end();
}

std::vector<std::wstring> cache_api::get_cache_names() const {
    std::vector<std::wstring> names;
    for (const auto& [n, _] : m_caches) names.push_back(n);
    return names;
}

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

    storage.entries[url] = entry;
    update_size(cache_name);
    ensure_quota(cache_name);

    return true;
}

bool cache_api::get(const std::wstring& cache_name, const std::wstring& url,
                     std::vector<uint8_t>& data,
                     std::map<std::wstring, std::wstring>& headers) {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return false;

    auto entry_it = cache_it->second.entries.find(url);
    if (entry_it == cache_it->second.entries.end()) return false;

    auto& entry = entry_it->second;
    if (entry.is_expired()) {
        cache_it->second.entries.erase(entry_it);
        update_size(cache_name);
        return false;
    }

    data = entry.response_data;
    headers = entry.response_headers;
    return true;
}

bool cache_api::remove(const std::wstring& cache_name, const std::wstring& url) {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return false;
    auto result = cache_it->second.entries.erase(url);
    if (result) update_size(cache_name);
    return result > 0;
}

void cache_api::clear(const std::wstring& cache_name) {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it != m_caches.end()) {
        cache_it->second.entries.clear();
        cache_it->second.total_size = 0;
    }
}

bool cache_api::match(const std::wstring& cache_name, const std::wstring& url,
                       std::vector<uint8_t>& data,
                       const cache_query_options& options) {
    (void)options;
    std::map<std::wstring, std::wstring> headers;
    return get(cache_name, url, data, headers);
}

std::vector<std::wstring> cache_api::match_all(const std::wstring& cache_name, const std::wstring& url,
                                                 const cache_query_options& options) {
    std::vector<std::wstring> results;
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return results;

    for (const auto& [entry_url, entry] : cache_it->second.entries) {
        if (url.empty() || entry_url == url) {
            if (!entry.is_expired()) results.push_back(entry_url);
        }
    }

    return results;
}

std::vector<cache_batch_result> cache_api::batch(const std::wstring& cache_name,
                                                   const std::vector<cache_batch_operation>& operations) {
    std::vector<cache_batch_result> results;
    for (const auto& op : operations) {
        cache_batch_result result;
        switch (op.type) {
            case cache_batch_operation::GET: {
                std::vector<uint8_t> data;
                std::map<std::wstring, std::wstring> headers;
                if (get(cache_name, op.request_url, data, headers)) {
                    result.success = true;
                    result.data = data;
                    result.headers = headers;
                }
                break;
            }
            case cache_batch_operation::PUT:
                result.success = put(cache_name, op.request_url, op.response_data,
                                      op.response_headers, op.response_status_code,
                                      op.response_status_text);
                break;
            case cache_batch_operation::DELETE:
                result.success = remove(cache_name, op.request_url);
                break;
        }
        results.push_back(result);
    }
    return results;
}

std::vector<std::wstring> cache_api::keys(const std::wstring& cache_name) const {
    auto cache_it = m_caches.find(cache_name);
    if (cache_it == m_caches.end()) return {};
    std::vector<std::wstring> result;
    for (const auto& [url, _] : cache_it->second.entries) {
        (void)_;
        result.push_back(url);
    }
    return result;
}

uint64_t cache_api::get_cache_size(const std::wstring& cache_name) const {
    auto cache_it = m_caches.find(cache_name);
    return cache_it != m_caches.end() ? cache_it->second.total_size : 0;
}

uint64_t cache_api::get_total_size() const {
    uint64_t total = 0;
    for (const auto& [_, storage] : m_caches) total += storage.total_size;
    return total;
}

void cache_api::evict_expired() {
    for (auto& [cache_name, storage] : m_caches) {
        for (auto it = storage.entries.begin(); it != storage.entries.end(); ) {
            if (it->second.is_expired()) it = storage.entries.erase(it);
            else ++it;
        }
        update_size(cache_name);
    }
}

void cache_api::evict_oldest(uint64_t target_bytes) {
    for (auto& [cache_name, storage] : m_caches) {
        if (storage.total_size <= target_bytes) continue;
        std::vector<std::pair<std::wstring, cache_entry_ext*>> sorted;
        for (auto& [url, entry] : storage.entries) {
            sorted.push_back({url, &entry});
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.second->stored_at < b.second->stored_at;
        });
        for (const auto& [url, _] : sorted) {
            if (storage.total_size <= target_bytes) break;
            auto it = storage.entries.find(url);
            if (it != storage.entries.end()) {
                storage.total_size -= it->second.size();
                storage.entries.erase(it);
            }
        }
        update_size(cache_name);
    }
}

void cache_api::evict_all() {
    for (auto& [_, storage] : m_caches) {
        storage.entries.clear();
        storage.total_size = 0;
    }
}

bool cache_api::persist_to_disk(const std::wstring& path) {
    (void)path;
    return true;
}

bool cache_api::load_from_disk(const std::wstring& path) {
    (void)path;
    return true;
}

cache_api::cache_storage& cache_api::get_or_create(const std::wstring& cache_name) {
    return m_caches[cache_name];
}

void cache_api::update_size(const std::wstring& cache_name) {
    auto& storage = m_caches[cache_name];
    storage.total_size = 0;
    for (const auto& [_, entry] : storage.entries) {
        storage.total_size += entry.size();
    }
}

void cache_api::ensure_quota(const std::wstring& cache_name) {
    auto& storage = m_caches[cache_name];
    if (storage.total_size > m_max_size) {
        evict_oldest(m_max_size / 2);
    }
}

// ---- cache_manager --------------------------------------------------------

cache_manager& cache_manager::instance() {
    static cache_manager mgr;
    return mgr;
}

std::shared_ptr<cache_api> cache_manager::open(const std::wstring& cache_name) {
    if (m_caches.find(cache_name) == m_caches.end()) {
        auto cache = std::make_shared<cache_api>();
        cache->open_cache(cache_name);
        m_caches[cache_name] = cache;
    }
    return m_caches[cache_name];
}

bool cache_manager::has(const std::wstring& cache_name) const {
    return m_caches.find(cache_name) != m_caches.end();
}

bool cache_manager::delete_cache(const std::wstring& cache_name) {
    return m_caches.erase(cache_name) > 0;
}

std::vector<std::wstring> cache_manager::keys() const {
    std::vector<std::wstring> names;
    for (const auto& [n, _] : m_caches) names.push_back(n);
    return names;
}

bool cache_manager::match(const std::wstring& url, std::vector<uint8_t>& data) {
    for (auto& [_, cache] : m_caches) {
        std::map<std::wstring, std::wstring> headers;
        if (cache->get(L"default", url, data, headers)) return true;
    }
    return false;
}

} // namespace hre::storage
