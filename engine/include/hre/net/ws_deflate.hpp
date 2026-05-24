#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace hre::net {

struct ws_deflate_config {
    bool server_no_context_takeover = false;
    bool client_no_context_takeover = false;
    int server_max_window_bits = 15;
    int client_max_window_bits = 15;
};

class ws_deflate {
public:
    ws_deflate();
    ~ws_deflate();

    bool initialize(const ws_deflate_config& config);
    bool is_initialized() const;

    std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool fin);
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool fin);

    void reset_deflater();
    void reset_inflater();

    const ws_deflate_config& config() const { return config_; }

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
    ws_deflate_config config_;
    bool initialized_ = false;
};

} // namespace hre::net
