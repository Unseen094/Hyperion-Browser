#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <chrono>

namespace hre::services {

struct sync_registration {
    std::wstring tag;
    std::chrono::system_clock::time_point registered_at;
    uint32_t min_interval_seconds;
    uint32_t max_attempts;
    uint32_t attempt_count;
    bool one_shot;

    sync_registration()
        : min_interval_seconds(0)
        , max_attempts(3)
        , attempt_count(0)
        , one_shot(false)
    {}
};

class sync_manager {
public:
    static sync_manager& instance();

    bool register_sync(const std::wstring& tag, uint32_t min_interval = 0);
    bool unregister_sync(const std::wstring& tag);
    std::vector<std::wstring> get_sync_tags() const;
    bool has_sync(const std::wstring& tag) const;

    void on_sync_event(std::function<void(const std::wstring& tag)> cb) { m_sync_cb = cb; }
    void fire_sync(const std::wstring& tag);

    void process_pending_syncs();
    void clear_all();

private:
    sync_manager() = default;
    std::map<std::wstring, sync_registration> m_syncs;
    std::function<void(const std::wstring&)> m_sync_cb;
};

} // namespace hre::services
