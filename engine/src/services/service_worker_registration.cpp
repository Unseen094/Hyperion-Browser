#include "hre/services/service_worker_registration.hpp"
#include <algorithm>

namespace hre::services {

service_worker_registration::service_worker_registration(const std::wstring& scope)
    : m_scope(scope)
    , m_state(registration_state::REGISTERING)
    , m_last_update_check(std::chrono::steady_clock::now())
{}

service_worker_registration::~service_worker_registration() {
    unregister();
}

void service_worker_registration::update_state() {
    if (m_installing && m_installing->state() == worker_state::INSTALLED) {
        m_waiting = m_installing;
        m_installing.reset();
        m_state = registration_state::INSTALLED;
    }

    if (m_waiting && m_state == registration_state::INSTALLED) {
        m_active = m_waiting;
        m_waiting.reset();
        m_active->activate();
        m_state = registration_state::ACTIVATED;
    }

    if (m_active && m_active->state() == worker_state::ACTIVATED) {
        m_state = registration_state::ACTIVATED;
    } else if (m_active && m_active->state() == worker_state::REDUNDANT) {
        m_state = registration_state::REDUNDANT;
    }
}

void service_worker_registration::unregister() {
    if (m_installing) {
        m_installing->terminate();
        m_installing.reset();
    }
    if (m_waiting) {
        m_waiting->terminate();
        m_waiting.reset();
    }
    if (m_active) {
        m_active->terminate();
        m_active.reset();
    }
    m_state = registration_state::UNREGISTERED;
    if (m_unregistered_cb) m_unregistered_cb();
}

void service_worker_registration::update(const std::wstring& script_url) {
    auto now = std::chrono::steady_clock::now();
    if (now - m_last_update_check < UPDATE_INTERVAL)
        return;

    m_last_update_check = now;

    // Check for script update by fetching the script URL
    // If the script content changed, create a new worker
    auto new_worker = std::make_shared<service_worker>(script_url, m_scope);
    new_worker->set_state(worker_state::PARSING);

    update_found_event evt;
    evt.registration = shared_from_this();
    if (m_update_cb) m_update_cb(evt);

    set_installing(new_worker);
    new_worker->install();
    update_state();
}

void service_worker_registration::skip_waiting() {
    if (m_waiting) {
        m_waiting->activate();
        m_active = m_waiting;
        m_waiting.reset();
        update_state();
    }
}

} // namespace hre::services
