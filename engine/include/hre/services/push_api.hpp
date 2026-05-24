#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace hre::services {

struct push_subscription {
    std::wstring endpoint;
    std::wstring p256dh_key;
    std::wstring auth_key;
    std::wstring app_server_key;

    bool is_valid() const { return !endpoint.empty(); }
};

struct push_message {
    std::wstring title;
    std::wstring body;
    std::wstring icon;
    std::wstring tag;
    std::wstring data;
    std::map<std::wstring, std::wstring> actions;
};

class push_manager {
public:
    static push_manager& instance();

    void subscribe(const std::wstring& app_server_key,
                   std::function<void(const push_subscription&)> callback);
    void unsubscribe();
    bool is_subscribed() const { return m_subscription.is_valid(); }
    const push_subscription& current_subscription() const { return m_subscription; }

    void on_push_message(std::function<void(const push_message&)> cb) { m_push_cb = cb; }
    void simulate_push(const push_message& message);

    static bool is_push_supported();

private:
    push_manager() = default;
    push_subscription m_subscription;
    std::function<void(const push_message&)> m_push_cb;

    std::wstring generate_endpoint() const;
    std::wstring generate_key() const;
};

} // namespace hre::services
