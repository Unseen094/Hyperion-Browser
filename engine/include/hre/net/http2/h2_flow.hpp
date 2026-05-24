#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace hre::net::http2 {

class flow_controller {
public:
    explicit flow_controller(int32_t initial_window = 65535);

    int32_t current_window() const { return window_; }
    int32_t initial_window() const { return initial_window_; }

    bool can_send(int32_t frame_size) const;
    void consume(int32_t frame_size);
    void update_window(int32_t increment);

    bool is_blocked() const;
    void set_initial_window(int32_t size);

    void track_stream(uint32_t stream_id);
    void remove_stream(uint32_t stream_id);
    int32_t stream_window(uint32_t stream_id) const;
    bool stream_can_send(uint32_t stream_id, int32_t frame_size) const;
    void stream_consume(uint32_t stream_id, int32_t frame_size);
    void stream_update(uint32_t stream_id, int32_t increment);

private:
    int32_t initial_window_;
    int32_t window_;
    std::unordered_map<uint32_t, int32_t> stream_windows_;
};

} // namespace hre::net::http2
