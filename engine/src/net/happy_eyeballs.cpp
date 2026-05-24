#include "hre/net/happy_eyeballs.hpp"
#include <winsock2.h>
#include <winsock.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <functional>
#include <algorithm>

namespace hre::net {

class happy_eyeballs::impl {
public:
    std::vector<resolved_address> v4_addrs_;
    std::vector<resolved_address> v6_addrs_;
    std::chrono::milliseconds delay_{300};
    std::chrono::milliseconds timeout_{5000};
    std::atomic<bool> cancelled_{false};
    std::mutex mtx_;
    std::condition_variable cv_;

    int winner_fd_{-1};
    connection_result result_{connection_result::ALL_FAILED};
    bool completed_{false};

    struct attempt_state {
        int fd;
        resolved_address addr;
        bool finished;
        bool success;
    };

    std::pair<int, connection_result> do_connect(
        std::function<int(const resolved_address&)> connect_fn)
    {
        std::vector<attempt_state> attempts;
        std::queue<resolved_address> addr_queue;

        // IPv6 first, then IPv4 (RFC 8305 §4)
        for (const auto& a : v6_addrs_) addr_queue.push(a);
        for (const auto& a : v4_addrs_) addr_queue.push(a);

        if (addr_queue.empty()) {
            return {-1, connection_result::ADDRESS_INVALID};
        }

        auto start_time = std::chrono::steady_clock::now();
        size_t started_count = 0;

        while (!addr_queue.empty() && !cancelled_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
            if (elapsed >= timeout_) {
                break;
            }

            resolved_address addr = addr_queue.front();
            addr_queue.pop();

            attempt_state as;
            as.addr = addr;
            as.finished = false;
            as.success = false;
            as.fd = connect_fn(addr);
            attempts.push_back(as);
            started_count++;

            if (as.fd >= 0) {
                winner_fd_ = as.fd;
                result_ = connection_result::SUCCESS;
                completed_ = true;
                return {as.fd, connection_result::SUCCESS};
            }

            bool any_remaining = !addr_queue.empty();
            if (any_remaining) {
                std::this_thread::sleep_for(delay_);
            }
        }

        result_ = cancelled_ ? connection_result::TIMEOUT : connection_result::ALL_FAILED;
        completed_ = true;
        return {-1, result_};
    }
};

happy_eyeballs::happy_eyeballs() : pimpl_(std::make_unique<impl>()) {}
happy_eyeballs::~happy_eyeballs() = default;

void happy_eyeballs::set_addresses(std::vector<resolved_address> v4,
                                    std::vector<resolved_address> v6) {
    pimpl_->v4_addrs_ = std::move(v4);
    pimpl_->v6_addrs_ = std::move(v6);
}

void happy_eyeballs::set_connection_delay(std::chrono::milliseconds delay) {
    pimpl_->delay_ = delay;
}

void happy_eyeballs::set_connection_timeout(std::chrono::milliseconds timeout) {
    pimpl_->timeout_ = timeout;
}

std::pair<int, connection_result> happy_eyeballs::connect(
    std::function<int(const resolved_address&)> connect_fn)
{
    return pimpl_->do_connect(connect_fn);
}

void happy_eyeballs::cancel() {
    pimpl_->cancelled_ = true;
}

} // namespace hre::net
