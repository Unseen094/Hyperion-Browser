#include <hre/render/scroll_controller.hpp>
#include <algorithm>
#include <cmath>

namespace hre::render {

scroll_controller::scroll_controller() = default;

void scroll_controller::set_scroll_position(float x, float y) {
    m_position.x = clamp_scroll(x, m_max_scroll_x);
    m_position.y = clamp_scroll(y, m_max_scroll_y);
    m_position.target_x = m_position.x;
    m_position.target_y = m_position.y;
    m_position.velocity_x = 0.0f;
    m_position.velocity_y = 0.0f;
    m_position.is_animating = false;

    if (m_callback) {
        m_callback(m_position.x, m_position.y);
    }
}

void scroll_controller::scroll_to(float target_x, float target_y, uint32_t duration_ms) {
    m_position.target_x = clamp_scroll(target_x, m_max_scroll_x);
    m_position.target_y = clamp_scroll(target_y, m_max_scroll_y);

    if (duration_ms == 0) {
        set_scroll_position(m_position.target_x, m_position.target_y);
        return;
    }

    m_position.is_animating = true;
}

void scroll_controller::scroll_by(float delta_x, float delta_y) {
    float new_x = m_position.x + delta_x;
    float new_y = m_position.y + delta_y;

    if (!m_overscroll_enabled) {
        new_x = clamp_scroll(new_x, m_max_scroll_x);
        new_y = clamp_scroll(new_y, m_max_scroll_y);
    }

    set_scroll_position(new_x, new_y);
}

void scroll_controller::update(uint32_t delta_time_ms) {
    if (!m_position.is_animating) {
        return;
    }

    float dx = m_position.target_x - m_position.x;
    float dy = m_position.target_y - m_position.y;

    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < MIN_VELOCITY) {
        m_position.x = m_position.target_x;
        m_position.y = m_position.target_y;
        m_position.is_animating = false;
        m_position.velocity_x = 0.0f;
        m_position.velocity_y = 0.0f;

        if (m_callback) {
            m_callback(m_position.x, m_position.y);
        }
        return;
    }

    float t = std::min(1.0f, static_cast<float>(delta_time_ms) / 300.0f);
    float ease_t = t * t * (3.0f - 2.0f * t);

    float prev_x = m_position.x;
    float prev_y = m_position.y;

    m_position.x += dx * ease_t;
    m_position.y += dy * ease_t;

    m_position.velocity_x = m_position.x - prev_x;
    m_position.velocity_y = m_position.y - prev_y;

    if (m_callback) {
        m_callback(m_position.x, m_position.y);
    }
}

void scroll_controller::stop() {
    m_position.is_animating = false;
    m_position.velocity_x = 0.0f;
    m_position.velocity_y = 0.0f;
}

float scroll_controller::clamp_scroll(float value, float max) {
    if (value < 0.0f) {
        return m_overscroll_enabled ? value * RUBBER_BAND_FACTOR : 0.0f;
    }
    if (value > max) {
        return m_overscroll_enabled ? max + (value - max) * RUBBER_BAND_FACTOR : max;
    }
    return value;
}

} // namespace hre::render