#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include "hre/services/service_worker.hpp"

namespace hre::services {

enum class registration_state {
    REGISTERING,
    INSTALLING,
    INSTALLED,
    ACTIVATING,
    ACTIVATED,
    REDUNDANT,
    UNREGISTERED
};

struct update_found_event {
    std::shared_ptr<class service_worker_registration> registration;
};

class service_worker_registration : public std::enable_shared_from_this<service_worker_registration> {
public:
    service_worker_registration(const std::wstring& scope);
    ~service_worker_registration();

    const std::wstring& scope() const { return m_scope; }

    std::shared_ptr<service_worker> installing() const { return m_installing; }
    std::shared_ptr<service_worker> waiting() const { return m_waiting; }
    std::shared_ptr<service_worker> active() const { return m_active; }

    void set_installing(std::shared_ptr<service_worker> sw) { m_installing = sw; }
    void set_waiting(std::shared_ptr<service_worker> sw) { m_waiting = sw; }
    void set_active(std::shared_ptr<service_worker> sw) { m_active = sw; }

    registration_state state() const { return m_state; }
    void update_state();

    void unregister();
    void update(const std::wstring& script_url);
    void skip_waiting();

    bool is_unregistered() const { return m_state == registration_state::UNREGISTERED; }
    std::chrono::steady_clock::time_point last_update_check() const { return m_last_update_check; }

    void on_update_found(std::function<void(const update_found_event&)> cb) { m_update_cb = cb; }
    void on_unregistered(std::function<void()> cb) { m_unregistered_cb = cb; }

private:
    std::wstring m_scope;
    registration_state m_state;
    std::chrono::steady_clock::time_point m_last_update_check;

    std::shared_ptr<service_worker> m_installing;
    std::shared_ptr<service_worker> m_waiting;
    std::shared_ptr<service_worker> m_active;

    std::function<void(const update_found_event&)> m_update_cb;
    std::function<void()> m_unregistered_cb;

    static constexpr auto UPDATE_INTERVAL = std::chrono::hours(8);
};

} // namespace hre::services
