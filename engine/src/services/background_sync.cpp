#include "hre/services/background_sync.hpp"
#include <algorithm>

namespace hre::services {

sync_manager& sync_manager::instance() {
    static sync_manager mgr;
    return mgr;
}

bool sync_manager::register_sync(const std::wstring& tag, uint32_t min_interval) {
    if (tag.empty() || m_syncs.size() >= 100) return false;

    auto it = m_syncs.find(tag);
    if (it != m_syncs.end()) {
        it->second.min_interval_seconds = min_interval;
        it->second.attempt_count = 0;
        return true;
    }

    sync_registration reg;
    reg.tag = tag;
    reg.registered_at = std::chrono::system_clock::now();
    reg.min_interval_seconds = min_interval;
    reg.max_attempts = 3;
    reg.attempt_count = 0;
    reg.one_shot = true;
    m_syncs[tag] = reg;
    return true;
}

bool sync_manager::unregister_sync(const std::wstring& tag) {
    auto it = m_syncs.find(tag);
    if (it == m_syncs.end()) return false;
    m_syncs.erase(it);
    return true;
}

std::vector<std::wstring> sync_manager::get_sync_tags() const {
    std::vector<std::wstring> tags;
    for (const auto& [tag, reg] : m_syncs) {
        tags.push_back(tag);
    }
    return tags;
}

bool sync_manager::has_sync(const std::wstring& tag) const {
    return m_syncs.find(tag) != m_syncs.end();
}

void sync_manager::fire_sync(const std::wstring& tag) {
    auto it = m_syncs.find(tag);
    if (it == m_syncs.end()) return;

    it->second.attempt_count++;

    if (m_sync_cb) {
        m_sync_cb(tag);
    }

    if (it->second.one_shot || it->second.attempt_count >= it->second.max_attempts) {
        m_syncs.erase(it);
    }
}

void sync_manager::process_pending_syncs() {
    auto now = std::chrono::system_clock::now();

    // Create a copy of tags since fire_sync may erase
    auto tags = get_sync_tags();
    for (const auto& tag : tags) {
        auto it = m_syncs.find(tag);
        if (it == m_syncs.end()) continue;

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.registered_at).count();
        if (elapsed >= it->second.min_interval_seconds) {
            fire_sync(tag);
        }
    }
}

void sync_manager::clear_all() {
    m_syncs.clear();
}

} // namespace hre::services
