#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace hre::net {

struct resolved_address {
    std::string address;
    int family; // AF_INET or AF_INET6
    int port;
};

enum class connection_result {
    SUCCESS,
    TIMEOUT,
    CONNECTION_REFUSED,
    NETWORK_UNREACHABLE,
    ADDRESS_INVALID,
    ALL_FAILED
};

class happy_eyeballs {
public:
    happy_eyeballs();
    ~happy_eyeballs();

    void set_addresses(std::vector<resolved_address> v4,
                       std::vector<resolved_address> v6);

    void set_connection_delay(std::chrono::milliseconds delay);
    void set_connection_timeout(std::chrono::milliseconds timeout);

    std::pair<int, connection_result> connect(
        std::function<int(const resolved_address&)> connect_fn);

    void cancel();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace hre::net
