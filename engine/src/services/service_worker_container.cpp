#include "hre/services/service_worker_container.hpp"
#include <algorithm>
#include <sstream>

namespace hre::services {

service_worker_container& service_worker_container::instance() {
    static service_worker_container container;
    return container;
}

void service_worker_container::register_worker(
    const std::wstring& script_url, const std::wstring& scope,
    std::function<void(std::shared_ptr<service_worker_registration>)> callback) {

    auto existing = get_registration(scope);
    if (existing) {
        existing->update(script_url);
        if (callback) callback(existing);
        return;
    }

    auto registration = std::make_shared<service_worker_registration>(scope);

    auto worker = std::make_shared<service_worker>(script_url, scope);

    worker->on_state_change([registration, this, scope](worker_state state) {
        registration->update_state();
        if (state == worker_state::ACTIVATED) {
            if (m_reg_cb) m_reg_cb(scope);
        }
    });

    registration->set_installing(worker);
    worker->install();
    m_registrations[scope] = registration;

    if (callback) callback(registration);
}

void service_worker_container::unregister_worker(const std::wstring& scope) {
    auto it = m_registrations.find(scope);
    if (it != m_registrations.end()) {
        it->second->unregister();
        m_registrations.erase(it);
    }
}

std::shared_ptr<service_worker_registration> service_worker_container::get_registration(
    const std::wstring& scope) const {
    auto it = m_registrations.find(scope);
    return (it != m_registrations.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<service_worker_registration>> service_worker_container::get_registrations() const {
    std::vector<std::shared_ptr<service_worker_registration>> regs;
    for (const auto& [scope, reg] : m_registrations) {
        regs.push_back(reg);
    }
    return regs;
}

std::shared_ptr<service_worker> service_worker_container::get_active_worker(
    const std::wstring& url) const {
    std::wstring scope = find_best_scope_match(url);
    if (scope.empty()) return nullptr;
    auto reg = get_registration(scope);
    return reg ? reg->active() : nullptr;
}

std::shared_ptr<service_worker> service_worker_container::get_worker_by_url(
    const std::wstring& script_url) const {
    for (const auto& [scope, reg] : m_registrations) {
        if (reg->active() && reg->active()->url() == script_url)
            return reg->active();
        if (reg->waiting() && reg->waiting()->url() == script_url)
            return reg->waiting();
        if (reg->installing() && reg->installing()->url() == script_url)
            return reg->installing();
    }
    return nullptr;
}

bool service_worker_container::handle_fetch_for_url(const std::wstring& url, fetch_event& event) {
    auto worker = get_active_worker(url);
    if (!worker) return false;
    return worker->handle_fetch_event(event);
}

void service_worker_container::check_for_updates() {
    for (auto& [scope, reg] : m_registrations) {
        if (reg->active()) {
            reg->update(reg->active()->url());
        }
    }
}

void service_worker_container::terminate_all() {
    for (auto& [scope, reg] : m_registrations) {
        if (reg->active()) reg->active()->terminate();
    }
    m_registrations.clear();
}

std::wstring service_worker_container::find_best_scope_match(const std::wstring& url) const {
    std::wstring best_match;
    size_t best_len = 0;

    for (const auto& [scope, reg] : m_registrations) {
        if (scope_matches(scope, url) && scope.size() > best_len) {
            best_match = scope;
            best_len = scope.size();
        }
    }
    return best_match;
}

bool service_worker_container::scope_matches(const std::wstring& scope, const std::wstring& url) const {
    if (scope.empty()) return true;
    if (url.size() < scope.size()) return false;
    return url.compare(0, scope.size(), scope) == 0;
}

} // namespace hre::services
