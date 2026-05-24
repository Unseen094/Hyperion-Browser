#pragma once

#include <cstdint>
#include <functional>
#include <chrono>

namespace hre::render {

struct scroll_position {
    float x = 0.0f;
    float y = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;
    bool is_animating = false;
};

class scroll_controller {
public:
    using scroll_callback = std::function<void(float x, float y)>;

    scroll_controller();

    void set_scroll_position(float x, float y);
    void scroll_to(float target_x, float target_y, uint32_t duration_ms = 300);
    void scroll_by(float delta_x, float delta_y);

    void set_callback(scroll_callback cb) { m_callback = cb; }

    scroll_position get_position() const { return m_position; }

    void update(uint32_t delta_time_ms);

    void set_max_scroll(float max_x, float max_y) {
        m_max_scroll_x = max_x;
        m_max_scroll_y = max_y;
    }

    float max_scroll_x() const { return m_max_scroll_x; }
    float max_scroll_y() const { return m_max_scroll_y; }

    bool is_animating() const { return m_position.is_animating; }
    void stop();

    void set_overscroll_enabled(bool enabled) { m_overscroll_enabled = enabled; }
    bool overscroll_enabled() const { return m_overscroll_enabled; }

private:
    float clamp_scroll(float value, float max);

    scroll_position m_position;
    scroll_callback m_callback;

    float m_max_scroll_x = 0.0f;
    float m_max_scroll_y = 0.0f;

    bool m_overscroll_enabled = true;

    uint64_t m_last_update_time = 0;

    static constexpr float FRICTION = 0.95f;
    static constexpr float MIN_VELOCITY = 0.1f;
    static constexpr float RUBBER_BAND_FACTOR = 0.55f;
};

} // namespace hre::render