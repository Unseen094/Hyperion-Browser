#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <chrono>

namespace hre::services {

struct fetch_event {
    std::wstring request_url;
    std::wstring request_method;
    std::map<std::wstring, std::wstring> headers;
    std::vector<uint8_t> body;

    bool responded = false;
    std::vector<uint8_t> response_data;
    int response_status = 200;
    std::wstring response_status_text = L"OK";
    std::map<std::wstring, std::wstring> response_headers;
};

struct service_worker_event {
    enum type_t {
        INSTALL,
        ACTIVATE,
        FETCH,
        MESSAGE,
        SYNC,
        PUSH,
        INSTALLATION_FAILED
    };
    type_t type;
    std::shared_ptr<fetch_event> fetch;
    std::wstring data;
};

enum class worker_state {
    PARSING,
    INSTALLING,
    INSTALLED,
    ACTIVATING,
    ACTIVATED,
    REDUNDANT
};

class service_worker {
public:
    service_worker(const std::wstring& url, const std::wstring& scope);
    ~service_worker();

    const std::wstring& url() const { return m_url; }
    const std::wstring& scope() const { return m_scope; }
    worker_state state() const { return m_state; }
    void set_state(worker_state s) { m_state = s; }

    void install();
    void activate();
    void dispatch_event(const service_worker_event& event);

    bool matches_scope(const std::wstring& url) const;

    void on_state_change(std::function<void(worker_state)> cb) { m_state_cb = cb; }
    void on_fetch(std::function<bool(fetch_event&)> cb) { m_fetch_cb = cb; }

    std::chrono::steady_clock::time_point install_time() const { return m_install_time; }
    void set_script(const std::wstring& script) { m_script = script; }
    const std::wstring& script() const { return m_script; }

    void terminate();
    bool is_running() const { return m_running; }

    bool handle_fetch_event(fetch_event& event);

private:
    std::wstring m_url;
    std::wstring m_scope;
    std::wstring m_script;
    worker_state m_state;
    bool m_running;

    std::chrono::steady_clock::time_point m_install_time;

    std::function<void(worker_state)> m_state_cb;
    std::function<bool(fetch_event&)> m_fetch_cb;

    void execute_script(const std::wstring& script_source);
    void handle_install_event();
    void handle_activate_event();
};

} // namespace hre::services
