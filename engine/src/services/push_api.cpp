#include "hre/services/push_api.hpp"
#include <sstream>
#include <random>
#include <iomanip>

namespace hre::services {

push_manager& push_manager::instance() {
    static push_manager mgr;
    return mgr;
}

void push_manager::subscribe(const std::wstring& app_server_key,
                              std::function<void(const push_subscription&)> callback) {
    if (m_subscription.is_valid()) {
        if (callback) callback(m_subscription);
        return;
    }

    m_subscription.endpoint = generate_endpoint();
    m_subscription.p256dh_key = generate_key();
    m_subscription.auth_key = generate_key();
    m_subscription.app_server_key = app_server_key;

    if (callback) callback(m_subscription);
}

void push_manager::unsubscribe() {
    m_subscription = push_subscription{};
}

void push_manager::simulate_push(const push_message& message) {
    if (m_push_cb) {
        m_push_cb(message);
    }
}

bool push_manager::is_push_supported() {
    return true;
}

std::wstring push_manager::generate_endpoint() const {
    std::random_device rd;
    std::wstringstream ss;
    ss << L"https://hyperion.push/v2/push/";
    for (int i = 0; i < 16; i++) {
        ss << std::hex << std::setw(2) << std::setfill(L'0') << rd();
    }
    return ss.str();
}

std::wstring push_manager::generate_key() const {
    std::random_device rd;
    std::wstring key;
    const wchar_t charset[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 43; i++) {
        key += charset[rd() % (sizeof(charset) / sizeof(wchar_t) - 1)];
    }
    return key;
}

} // namespace hre::services
