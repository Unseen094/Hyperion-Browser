#pragma once

#include <hre/net/response_cache.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <cstdint>

namespace hre::storage {

// ---- Cache Query ----------------------------------------------------------

struct cache_query_options {
    bool ignore_search = false;
    bool ignore_method = false;
    bool ignore_vary = false;
    std::wstring cache_name;
};

struct cache_batch_operation {
    enum op_type { PUT, DELETE, GET };
    op_type type = GET;
    std::wstring request_url;
    std::vector<uint8_t> response_data;
    std::map<std::wstring, std::wstring> response_headers;
    std::wstring response_status_text;
    int response_status_code = 200;
};

struct cache_batch_result {
    bool success = false;
    std::vector<uint8_t> data;
    std::map<std::wstring, std::wstring> headers;
};

// ---- Cache Entry (extended) -----------------------------------------------

struct cache_entry_ext {
    std::wstring request_url;
    std::wstring request_method = L"GET";

    std::vector<uint8_t> response_data;
    std::map<std::wstring, std::wstring> response_headers;
    int response_status_code = 200;
    std::wstring response_status_text = L"OK";

    std::chrono::system_clock::time_point stored_at;
    std::chrono::system_clock::time_point expires_at;
    std::wstring etag;
    std::wstring last_modified;
    std::wstring vary;

    int64_t size() const { return static_cast<int64_t>(response_data.size()); }
    bool is_expired() const;
    bool matches_vary(const std::map<std::wstring, std::wstring>& request_headers) const;
};

// ---- Cache API ------------------------------------------------------------

class cache_api {
public:
    cache_api();
    ~cache_api();

    // Named cache storage
    bool open_cache(const std::wstring& cache_name);
    bool delete_cache(const std::wstring& cache_name);
    bool has_cache(const std::wstring& cache_name) const;
    std::vector<std::wstring> get_cache_names() const;

    // Entry CRUD
    bool put(const std::wstring& cache_name, const std::wstring& url,
             const std::vector<uint8_t>& data,
             const std::map<std::wstring, std::wstring>& headers = {},
             int status_code = 200,
             const std::wstring& status_text = L"OK");

    bool get(const std::wstring& cache_name, const std::wstring& url,
             std::vector<uint8_t>& data,
             std::map<std::wstring, std::wstring>& headers);

    bool remove(const std::wstring& cache_name, const std::wstring& url);
    void clear(const std::wstring& cache_name);

    // Matching
    bool match(const std::wstring& cache_name, const std::wstring& url,
               std::vector<uint8_t>& data,
               const cache_query_options& options = {});

    std::vector<std::wstring> match_all(const std::wstring& cache_name, const std::wstring& url = L"",
                                         const cache_query_options& options = {});

    // Batch operations
    std::vector<cache_batch_result> batch(const std::wstring& cache_name,
                                           const std::vector<cache_batch_operation>& operations);

    // Keys
    std::vector<std::wstring> keys(const std::wstring& cache_name) const;

    // Storage management
    uint64_t get_cache_size(const std::wstring& cache_name) const;
    uint64_t get_total_size() const;
    void set_max_cache_size(uint64_t max_bytes) { m_max_size = max_bytes; }
    uint64_t get_max_cache_size() const { return m_max_size; }

    // Eviction
    void evict_expired();
    void evict_oldest(uint64_t target_bytes);
    void evict_all();

    // Persistence
    bool persist_to_disk(const std::wstring& path);
    bool load_from_disk(const std::wstring& path);

private:
    struct cache_storage {
        std::map<std::wstring, cache_entry_ext> entries;
        uint64_t total_size = 0;
    };

    std::map<std::wstring, cache_storage> m_caches;
    uint64_t m_max_size = 50 * 1024 * 1024; // 50 MB default

    cache_storage& get_or_create(const std::wstring& cache_name);
    void update_size(const std::wstring& cache_name);
    void ensure_quota(const std::wstring& cache_name);
};

// ---- Cache Manager (ServiceWorker-style) ----------------------------------

class cache_manager {
public:
    static cache_manager& instance();

    std::shared_ptr<cache_api> open(const std::wstring& cache_name);
    bool has(const std::wstring& cache_name) const;
    bool delete_cache(const std::wstring& cache_name);
    std::vector<std::wstring> keys() const;

    // Match across all caches
    bool match(const std::wstring& url, std::vector<uint8_t>& data);

private:
    cache_manager() = default;
    std::map<std::wstring, std::shared_ptr<cache_api>> m_caches;
};

} // namespace hre::storage
