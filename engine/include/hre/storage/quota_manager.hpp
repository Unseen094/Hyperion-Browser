#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <cstdint>

namespace hre::storage {

// ---- Storage Type ---------------------------------------------------------

enum class storage_type {
    LOCAL_STORAGE,
    SESSION_STORAGE,
    INDEXEDDB,
    CACHE_API,
    FILE_SYSTEM,
    COOKIES,
    SERVICE_WORKER,
    TOTAL
};

struct storage_usage_info {
    storage_type type = storage_type::TOTAL;
    std::wstring origin;
    uint64_t usage_bytes = 0;
    uint64_t quota_bytes = 0;
    double usage_percent = 0.0;
    int64_t last_accessed = 0;
};

// ---- Quota Manager --------------------------------------------------------

class quota_manager {
public:
    quota_manager();
    ~quota_manager();

    static quota_manager& instance();

    // Quota configuration per origin
    void set_origin_quota(const std::wstring& origin, uint64_t max_bytes);
    uint64_t get_origin_quota(const std::wstring& origin) const;
    bool has_origin_quota(const std::wstring& origin) const;

    // Temp storage quota (shared across all origins)
    void set_temporary_storage_quota(uint64_t max_bytes);
    uint64_t get_temporary_storage_quota() const { return m_temp_quota; }

    // Persistent storage quota
    void set_persistent_storage_quota(uint64_t max_bytes) { m_persistent_quota = max_bytes; }
    uint64_t get_persistent_storage_quota() const { return m_persistent_quota; }

    // Grand total
    void set_total_global_quota(uint64_t max_bytes) { m_global_quota = max_bytes; }
    uint64_t get_total_global_quota() const { return m_global_quota; }

    // Usage tracking
    void record_usage(const std::wstring& origin, storage_type type, int64_t delta_bytes);
    void set_usage(const std::wstring& origin, storage_type type, uint64_t bytes);
    uint64_t get_usage(const std::wstring& origin, storage_type type) const;
    uint64_t get_total_usage(const std::wstring& origin) const;
    uint64_t get_global_total_usage() const;

    // Quota check
    bool check_quota(const std::wstring& origin, uint64_t additional_bytes, storage_type type) const;
    bool is_origin_under_quota(const std::wstring& origin) const;
    bool is_global_under_quota(uint64_t additional_bytes = 0) const;

    // Eviction
    struct eviction_candidate {
        std::wstring origin;
        uint64_t usage_bytes = 0;
        int64_t last_accessed = 0;
    };

    std::vector<eviction_candidate> get_eviction_candidates(uint64_t target_bytes) const;
    void evict_origin(const std::wstring& origin);

    // Usage statistics
    std::vector<storage_usage_info> get_all_usage_info() const;
    storage_usage_info get_origin_usage_info(const std::wstring& origin) const;

    // Pressure handling
    enum class pressure_level { NONE, MODERATE, CRITICAL };
    pressure_level get_pressure_level() const;
    using pressure_callback = std::function<void(pressure_level)>;
    void set_pressure_callback(pressure_callback cb) { m_pressure_cb = cb; }

    // Reset
    void reset_origin(const std::wstring& origin);
    void reset_all();

private:
    struct origin_quota {
        uint64_t quota = UINT64_MAX;
        std::map<storage_type, uint64_t> usage;
        int64_t last_accessed = 0;
    };

    std::map<std::wstring, origin_quota> m_origins;
    uint64_t m_global_quota = UINT64_MAX;
    uint64_t m_temp_quota = 500 * 1024 * 1024;  // 500 MB
    uint64_t m_persistent_quota = 50 * 1024 * 1024; // 50 MB
    pressure_callback m_pressure_cb;

    void update_pressure();
    static std::wstring storage_type_to_string(storage_type t);
};

// ---- Storage Pressure Manager ---------------------------------------------

class storage_pressure_manager {
public:
    storage_pressure_manager();
    ~storage_pressure_manager();

    void start_monitoring();
    void stop_monitoring();
    bool is_monitoring() const { return m_monitoring; }

    using eviction_callback = std::function<bool(const std::wstring& origin)>;
    void set_eviction_callback(eviction_callback cb) { m_eviction_cb = cb; }

    uint64_t get_total_available_space() const;
    uint64_t get_total_used_space() const;
    double get_pressure_ratio() const;

    // Thresholds (percent of total)
    void set_warning_threshold(double pct) { m_warning_threshold = pct; }
    void set_critical_threshold(double pct) { m_critical_threshold = pct; }

private:
    bool m_monitoring = false;
    eviction_callback m_eviction_cb;
    double m_warning_threshold = 0.8;  // 80%
    double m_critical_threshold = 0.95; // 95%
};

} // namespace hre::storage
