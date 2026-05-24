#include "hre/net/request.hpp"
#include "hre/net/network_manager.hpp"
#include "hre/net/ssl_socket.hpp"
#include <hjs/runtime/promise.hpp>
#include <thread>
#include <codecvt>
#include <locale>

namespace hre::net {

std::wstring request::fetch(const std::wstring& url) {
    // Simplified: Just use the network manager for now
    hre::net::network_manager nm;
    auto result = nm.perform_request(url);
    return std::wstring(result.begin(), result.end());
}

} // namespace hre::net
