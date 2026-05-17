#pragma once

#include <string>
#include <functional>
#include <vector>
#include <map>

namespace hre::net {

class network_manager {
public:
    network_manager();
    ~network_manager();

    // Fetches a resource asynchronously
    void fetch_async(const std::wstring& url, std::function<void(const std::string&)> callback);

    // Synchronous internal core
    std::string perform_request(const std::wstring& url);

private:
    // Foundation for future SSL/TLS support (OpenSSL or WinTLS integration point)
    bool m_use_ssl = false;
};

} // namespace hre::net
