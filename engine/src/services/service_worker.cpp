#include "hre/services/service_worker.hpp"
#include <sstream>
#include <algorithm>
#include <thread>

namespace hre::services {

service_worker::service_worker(const std::wstring& url, const std::wstring& scope)
    : m_url(url)
    , m_scope(scope)
    , m_state(worker_state::PARSING)
    , m_running(false)
{}

service_worker::~service_worker() {
    terminate();
}

bool service_worker::matches_scope(const std::wstring& url) const {
    if (m_scope.empty()) return true;
    if (url.size() < m_scope.size()) return false;
    return url.compare(0, m_scope.size(), m_scope) == 0;
}

void service_worker::install() {
    m_state = worker_state::INSTALLING;
    m_install_time = std::chrono::steady_clock::now();

    service_worker_event event;
    event.type = service_worker_event::INSTALL;
    dispatch_event(event);

    m_state = worker_state::INSTALLED;
    if (m_state_cb) m_state_cb(m_state);
}

void service_worker::activate() {
    m_state = worker_state::ACTIVATING;

    service_worker_event event;
    event.type = service_worker_event::ACTIVATE;
    dispatch_event(event);

    m_state = worker_state::ACTIVATED;
    m_running = true;
    if (m_state_cb) m_state_cb(m_state);
}

void service_worker::dispatch_event(const service_worker_event& event) {
    switch (event.type) {
        case service_worker_event::INSTALL:
            handle_install_event();
            break;
        case service_worker_event::ACTIVATE:
            handle_activate_event();
            break;
        case service_worker_event::FETCH:
            if (event.fetch) {
                handle_fetch_event(*event.fetch);
            }
            break;
        default:
            break;
    }
}

bool service_worker::handle_fetch_event(fetch_event& event) {
    if (m_fetch_cb) {
        return m_fetch_cb(event);
    }

    // Default: pass through (don't intercept)
    event.responded = false;
    return false;
}

void service_worker::terminate() {
    m_running = false;
    m_state = worker_state::REDUNDANT;
    if (m_state_cb) m_state_cb(m_state);
}

void service_worker::execute_script(const std::wstring& script_source) {
    // The HJS runtime would execute the service worker script here
    // This sets up event listeners (install, activate, fetch, etc.)
}

void service_worker::handle_install_event() {
    // During install, the worker caches resources
    // self.addEventListener('install', function(event) { ... })
}

void service_worker::handle_activate_event() {
    // During activate, the worker cleans up old caches
    // self.addEventListener('activate', function(event) { ... })
}

} // namespace hre::services
