#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include "hre/services/service_worker.hpp"
#include "hre/services/service_worker_registration.hpp"

namespace hre::services {

class service_worker_container {
public:
    static service_worker_container& instance();

    void register_worker(const std::wstring& script_url, const std::wstring& scope,
                         std::function<void(std::shared_ptr<service_worker_registration>)> callback);
    void unregister_worker(const std::wstring& scope);

    std::shared_ptr<service_worker_registration> get_registration(const std::wstring& scope) const;
    std::vector<std::shared_ptr<service_worker_registration>> get_registrations() const;

    std::shared_ptr<service_worker> get_active_worker(const std::wstring& url) const;
    std::shared_ptr<service_worker> get_worker_by_url(const std::wstring& script_url) const;

    bool handle_fetch_for_url(const std::wstring& url, fetch_event& event);

    void check_for_updates();
    void terminate_all();

    void on_registration_complete(std::function<void(const std::wstring&)> cb) { m_reg_cb = cb; }

    size_t worker_count() const { return m_registrations.size(); }

private:
    service_worker_container() = default;
    std::map<std::wstring, std::shared_ptr<service_worker_registration>> m_registrations;

    std::function<void(const std::wstring&)> m_reg_cb;

    std::wstring find_best_scope_match(const std::wstring& url) const;
    bool scope_matches(const std::wstring& scope, const std::wstring& url) const;
};

} // namespace hre::services
