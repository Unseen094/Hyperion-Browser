#include "hre/net/http2/h2_flow.hpp"
#include <algorithm>
#include <limits>

namespace hre::net::http2 {

flow_controller::flow_controller(int32_t initial_window)
    : initial_window_(initial_window)
    , window_(initial_window)
{}

bool flow_controller::can_send(int32_t frame_size) const {
    return window_ >= frame_size;
}

void flow_controller::consume(int32_t frame_size) {
    window_ -= frame_size;
}

void flow_controller::update_window(int32_t increment) {
    int64_t result = static_cast<int64_t>(window_) + increment;
    if (result > std::numeric_limits<int32_t>::max()) {
        window_ = std::numeric_limits<int32_t>::max();
    } else if (result < 0) {
        window_ = 0;
    } else {
        window_ = static_cast<int32_t>(result);
    }
}

bool flow_controller::is_blocked() const {
    return window_ <= 0;
}

void flow_controller::set_initial_window(int32_t size) {
    int32_t delta = size - initial_window_;
    initial_window_ = size;
    window_ += delta;
    for (auto& [id, w] : stream_windows_) {
        w += delta;
    }
}

void flow_controller::track_stream(uint32_t stream_id) {
    stream_windows_.emplace(stream_id, initial_window_);
}

void flow_controller::remove_stream(uint32_t stream_id) {
    stream_windows_.erase(stream_id);
}

int32_t flow_controller::stream_window(uint32_t stream_id) const {
    auto it = stream_windows_.find(stream_id);
    return it != stream_windows_.end() ? it->second : -1;
}

bool flow_controller::stream_can_send(uint32_t stream_id, int32_t frame_size) const {
    auto it = stream_windows_.find(stream_id);
    if (it == stream_windows_.end()) return false;
    return it->second >= frame_size;
}

void flow_controller::stream_consume(uint32_t stream_id, int32_t frame_size) {
    auto it = stream_windows_.find(stream_id);
    if (it != stream_windows_.end()) {
        it->second -= frame_size;
    }
}

void flow_controller::stream_update(uint32_t stream_id, int32_t increment) {
    auto it = stream_windows_.find(stream_id);
    if (it != stream_windows_.end()) {
        int64_t result = static_cast<int64_t>(it->second) + increment;
        if (result > std::numeric_limits<int32_t>::max()) {
            it->second = std::numeric_limits<int32_t>::max();
        } else if (result < 0) {
            it->second = 0;
        } else {
            it->second = static_cast<int32_t>(result);
        }
    }
}

} // namespace hre::net::http2
