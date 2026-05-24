#include <hre/storage/quota_manager.hpp>
#include <algorithm>
#include <sstream>

namespace hre::storage {

quota_manager::quota_manager() = default;
quota_manager::~quota_manager() = default;

quota_manager& quota_manager::instance() {
    static quota_manager mgr;
    return mgr;
}

void quota_manager::set_origin_quota(const std::wstring& origin, uint64_t max_bytes) {
    m_origins[origin].quota = max_bytes;
}

uint64_t quota_manager::get_origin_quota(const std::wstring& origin) const {
    auto it = m_origins.find(origin);
    return it != m_origins.end() ? it->second.quota : UINT64_MAX;
}

bool quota_manager::has_origin_quota(const std::wstring& origin) const {
    return m_origins.find(origin) != m_origins.end();
}

void quota_manager::set_temporary_storage_quota(uint64_t max_bytes) {
    m_temp_quota = max_bytes;
}

void quota_manager::record_usage(const std::wstring& origin, storage_type type, int64_t delta_bytes) {
    auto& entry = m_origins[origin];
    if (delta_bytes >= 0 || entry.usage[type] >= static_cast<uint64_t>(-delta_bytes)) {
        if (delta_bytes >= 0) entry.usage[type] += delta_bytes;
        else entry.usage[type] -= -delta_bytes;
    } else {
        entry.usage[type] = 0;
    }
    entry.last_accessed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    update_pressure();
}

void quota_manager::set_usage(const std::wstring& origin, storage_type type, uint64_t bytes) {
    m_origins[origin].usage[type] = bytes;
    update_pressure();
}

uint64_t quota_manager::get_usage(const std::wstring& origin, storage_type type) const {
    auto it = m_origins.find(origin);
    if (it == m_origins.end()) return 0;
    auto usage_it = it->second.usage.find(type);
    return usage_it != it->second.usage.end() ? usage_it->second : 0;
}

uint64_t quota_manager::get_total_usage(const std::wstring& origin) const {
    auto it = m_origins.find(origin);
    if (it == m_origins.end()) return 0;
    uint64_t total = 0;
    for (const auto& [_, bytes] : it->second.usage) total += bytes;
    return total;
}

uint64_t quota_manager::get_global_total_usage() const {
    uint64_t total = 0;
    for (const auto& [_, entry] : m_origins) {
        for (const auto& [_, bytes] : entry.usage) total += bytes;
    }
    return total;
}

bool quota_manager::check_quota(const std::wstring& origin, uint64_t additional_bytes, storage_type type) const {
    auto it = m_origins.find(origin);
    uint64_t origin_total = 0;
    if (it != m_origins.end()) {
        for (const auto& [t, bytes] : it->second.usage) origin_total += bytes;
        if (origin_total + additional_bytes > it->second.quota) return false;
    }

    uint64_t global = get_global_total_usage();
    if (global + additional_bytes > m_global_quota) return false;

    return true;
}

bool quota_manager::is_origin_under_quota(const std::wstring& origin) const {
    return get_total_usage(origin) <= get_origin_quota(origin);
}

bool quota_manager::is_global_under_quota(uint64_t additional_bytes) const {
    return (get_global_total_usage() + additional_bytes) <= m_global_quota;
}

std::vector<quota_manager::eviction_candidate> quota_manager::get_eviction_candidates(uint64_t target_bytes) const {
    std::vector<eviction_candidate> candidates;
    for (const auto& [origin, entry] : m_origins) {
        uint64_t total = 0;
        for (const auto& [_, bytes] : entry.usage) total += bytes;
        if (total > 0) {
            candidates.push_back({ origin, total, entry.last_accessed });
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.last_accessed < b.last_accessed;
    });

    uint64_t freed = 0;
    std::vector<eviction_candidate> to_evict;
    for (const auto& c : candidates) {
        if (freed >= target_bytes) break;
        to_evict.push_back(c);
        freed += c.usage_bytes;
    }

    return to_evict;
}

void quota_manager::evict_origin(const std::wstring& origin) {
    auto it = m_origins.find(origin);
    if (it != m_origins.end()) {
        it->second.usage.clear();
        it->second.last_accessed = 0;
    }
    update_pressure();
}

std::vector<storage_usage_info> quota_manager::get_all_usage_info() const {
    std::vector<storage_usage_info> infos;
    for (const auto& [origin, entry] : m_origins) {
        for (const auto& [type, bytes] : entry.usage) {
            storage_usage_info info;
            info.origin = origin;
            info.type = type;
            info.usage_bytes = bytes;
            info.quota_bytes = entry.quota;
            info.usage_percent = entry.quota > 0 ? (static_cast<double>(bytes) / entry.quota) * 100.0 : 0.0;
            info.last_accessed = entry.last_accessed;
            infos.push_back(info);
        }
    }
    return infos;
}

storage_usage_info quota_manager::get_origin_usage_info(const std::wstring& origin) const {
    storage_usage_info info;
    info.origin = origin;
    info.quota_bytes = get_origin_quota(origin);
    info.usage_bytes = get_total_usage(origin);
    info.usage_percent = info.quota_bytes > 0 ? (static_cast<double>(info.usage_bytes) / info.quota_bytes) * 100.0 : 0.0;
    return info;
}

quota_manager::pressure_level quota_manager::get_pressure_level() const {
    double global_ratio = static_cast<double>(get_global_total_usage()) / m_global_quota;
    if (m_global_quota == UINT64_MAX) return pressure_level::NONE;
    if (global_ratio >= 0.95) return pressure_level::CRITICAL;
    if (global_ratio >= 0.80) return pressure_level::MODERATE;
    return pressure_level::NONE;
}

void quota_manager::update_pressure() {
    if (m_pressure_cb) m_pressure_cb(get_pressure_level());
}

void quota_manager::reset_origin(const std::wstring& origin) {
    m_origins.erase(origin);
    update_pressure();
}

void quota_manager::reset_all() {
    m_origins.clear();
    update_pressure();
}

std::wstring quota_manager::storage_type_to_string(storage_type t) {
    switch (t) {
        case storage_type::LOCAL_STORAGE: return L"local_storage";
        case storage_type::SESSION_STORAGE: return L"session_storage";
        case storage_type::INDEXEDDB: return L"indexeddb";
        case storage_type::CACHE_API: return L"cache_api";
        case storage_type::FILE_SYSTEM: return L"file_system";
        case storage_type::COOKIES: return L"cookies";
        case storage_type::SERVICE_WORKER: return L"service_worker";
        default: return L"total";
    }
}

// ---- storage_pressure_manager ---------------------------------------------

storage_pressure_manager::storage_pressure_manager() = default;
storage_pressure_manager::~storage_pressure_manager() { stop_monitoring(); }

void storage_pressure_manager::start_monitoring() { m_monitoring = true; }
void storage_pressure_manager::stop_monitoring() { m_monitoring = false; }

uint64_t storage_pressure_manager::get_total_available_space() const {
    return quota_manager::instance().get_total_global_quota();
}

uint64_t storage_pressure_manager::get_total_used_space() const {
    return quota_manager::instance().get_global_total_usage();
}

double storage_pressure_manager::get_pressure_ratio() const {
    uint64_t avail = get_total_available_space();
    if (avail == 0 || avail == UINT64_MAX) return 0.0;
    return static_cast<double>(get_total_used_space()) / avail;
}

} // namespace hre::storage
