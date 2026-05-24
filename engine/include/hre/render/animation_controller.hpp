#pragma once

#include <functional>
#include <map>
#include <string>
#include <chrono>
#include <cstdint>

namespace hre::render {

using clock = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<clock>;

enum class animation_fill_mode {
    NONE,
    FORWARDS,
    BACKWARDS,
    BOTH
};

enum class animation_direction {
    NORMAL,
    REVERSE,
    ALTERNATE,
    ALTERNATE_REVERSE
};

enum class animation_play_state {
    PLAYING,
    PAUSED
};

struct keyframe {
    float time; // 0.0 to 1.0
    float value;
    std::wstring easing; // e.g., L"ease", L"linear", L"ease-in-out"
};

struct computed_animation_properties {
    float from_value = 0.0f;
    float to_value = 0.0f;
    float duration_ms = 300.0f;
    float delay_ms = 0.0f;
    animation_fill_mode fill_mode = animation_fill_mode::NONE;
    animation_direction direction = animation_direction::NORMAL;
    animation_play_state play_state = animation_play_state::PLAYING;
    float iteration_count = 1.0f; // -1 for infinite
};

class animation_controller {
public:
    using animation_end_callback = std::function<void(const std::wstring& property)>;

    animation_controller();

    uint32_t add_animation(
        const std::wstring& element_id,
        const std::wstring& property,
        float from_value,
        float to_value,
        uint32_t duration_ms,
        uint32_t delay_ms = 0,
        const std::wstring& easing = L"ease"
    );

    void add_keyframe(uint32_t anim_id, const keyframe& kf);
    void set_iteration_count(uint32_t anim_id, float count);
    void set_direction(uint32_t anim_id, animation_direction dir);
    void set_fill_mode(uint32_t anim_id, animation_fill_mode mode);
    void set_play_state(uint32_t anim_id, animation_play_state state);

    void pause(uint32_t anim_id);
    void play(uint32_t anim_id);
    void cancel(uint32_t anim_id);

    void set_end_callback(animation_end_callback cb) { m_end_callback = cb; }

    void update();

    bool is_animating(const std::wstring& element_id, const std::wstring& property) const;

    float get_current_value(uint32_t anim_id) const;

    void clear_animations_for_element(const std::wstring& element_id);

private:
    struct animation {
        std::wstring element_id;
        std::wstring property;
        float from_value = 0.0f;
        float to_value = 0.0f;
        float duration_ms = 300.0f;
        float delay_ms = 0.0f;
        time_point start_time;
        time_point pause_time;
        animation_direction direction = animation_direction::NORMAL;
        animation_fill_mode fill_mode = animation_fill_mode::NONE;
        animation_play_state play_state = animation_play_state::PLAYING;
        float iteration_count = 1.0f;
        float current_iteration = 0.0f;
        float current_time_ms = 0.0f;
        std::wstring easing;
        std::vector<keyframe> keyframes;
        bool cancelled = false;
    };

    float apply_easing(float t, const std::wstring& easing);

    std::map<uint32_t, animation> m_animations;
    uint32_t m_next_id = 1;
    animation_end_callback m_end_callback;
};

} // namespace hre::render