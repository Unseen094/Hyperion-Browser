#include <hre/render/animation_controller.hpp>
#include <cmath>
#include <algorithm>

namespace hre::render {

animation_controller::animation_controller() = default;

uint32_t animation_controller::add_animation(
    const std::wstring& element_id,
    const std::wstring& property,
    float from_value,
    float to_value,
    uint32_t duration_ms,
    uint32_t delay_ms,
    const std::wstring& easing
) {
    animation anim;
    anim.element_id = element_id;
    anim.property = property;
    anim.from_value = from_value;
    anim.to_value = to_value;
    anim.duration_ms = static_cast<float>(duration_ms);
    anim.delay_ms = static_cast<float>(delay_ms);
    anim.start_time = clock::now();
    anim.easing = easing;

    uint32_t id = m_next_id++;
    m_animations[id] = std::move(anim);
    return id;
}

void animation_controller::add_keyframe(uint32_t anim_id, const keyframe& kf) {
    auto it = m_animations.find(anim_id);
    if (it != m_animations.end()) {
        it->second.keyframes.push_back(kf);
        std::sort(it->second.keyframes.begin(), it->second.keyframes.end(),
            [](const keyframe& a, const keyframe& b) { return a.time < b.time; });
    }
}

void animation_controller::set_iteration_count(uint32_t anim_id, float count) {
    auto it = m_animations.find(anim_id);
    if (it != m_animations.end()) {
        it->second.iteration_count = count;
    }
}

void animation_controller::set_direction(uint32_t anim_id, animation_direction dir) {
    auto it = m_animations.find(anim_id);
    if (it != m_animations.end()) {
        it->second.direction = dir;
    }
}

void animation_controller::set_fill_mode(uint32_t anim_id, animation_fill_mode mode) {
    auto it = m_animations.find(anim_id);
    if (it != m_animations.end()) {
        it->second.fill_mode = mode;
    }
}

void animation_controller::set_play_state(uint32_t anim_id, animation_play_state state) {
    auto it = m_animations.find(anim_id);
    if (it != m_animations.end()) {
        if (state == animation_play_state::PAUSED && it->second.play_state == animation_play_state::PLAYING) {
            it->second.pause_time = clock::now();
        } else if (state == animation_play_state::PLAYING && it->second.play_state == animation_play_state::PAUSED) {
            auto pause_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                clock::now() - it->second.pause_time).count();
            it->second.start_time += std::chrono::milliseconds(pause_duration);
        }
        it->second.play_state = state;
    }
}

void animation_controller::pause(uint32_t anim_id) {
    set_play_state(anim_id, animation_play_state::PAUSED);
}

void animation_controller::play(uint32_t anim_id) {
    set_play_state(anim_id, animation_play_state::PLAYING);
}

void animation_controller::cancel(uint32_t anim_id) {
    auto it = m_animations.find(anim_id);
    if (it != m_animations.end()) {
        it->second.cancelled = true;
    }
}

void animation_controller::update() {
    auto now = clock::now();
    std::vector<uint32_t> to_remove;

    for (auto& [id, anim] : m_animations) {
        if (anim.cancelled) {
            to_remove.push_back(id);
            continue;
        }

        if (anim.play_state == animation_play_state::PAUSED) {
            continue;
        }

        float elapsed = static_cast<float>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - anim.start_time).count());

        float total_duration = anim.duration_ms + anim.delay_ms;
        if (elapsed < anim.delay_ms) {
            continue;
        }

        anim.current_time_ms = elapsed - anim.delay_ms;

        bool reverse = false;
        if (anim.direction == animation_direction::REVERSE) {
            reverse = true;
        } else if (anim.direction == animation_direction::ALTERNATE) {
            float iterations_completed = anim.current_time_ms / anim.duration_ms;
            reverse = (static_cast<int>(iterations_completed) % 2 == 1);
        } else if (anim.direction == animation_direction::ALTERNATE_REVERSE) {
            float iterations_completed = anim.current_time_ms / anim.duration_ms;
            reverse = (static_cast<int>(iterations_completed) % 2 == 0);
        }

        float t = anim.current_time_ms / anim.duration_ms;
        t = std::max(0.0f, std::min(1.0f, t));

        if (reverse) {
            t = 1.0f - t;
        }

        float eased_t = apply_easing(t, anim.easing);
        anim.to_value = anim.from_value + (anim.to_value - anim.from_value) * eased_t;

        if (anim.current_time_ms >= anim.duration_ms) {
            if (anim.iteration_count < 0 || anim.current_iteration + 1 < anim.iteration_count) {
                anim.current_iteration++;
                anim.current_time_ms = 0;
                anim.start_time = now;
            } else {
                to_remove.push_back(id);
                if (m_end_callback) {
                    m_end_callback(anim.property);
                }
            }
        }
    }

    for (auto id : to_remove) {
        m_animations.erase(id);
    }
}

bool animation_controller::is_animating(const std::wstring& element_id, const std::wstring& property) const {
    for (const auto& [id, anim] : m_animations) {
        if (anim.element_id == element_id && anim.property == property && !anim.cancelled) {
            return true;
        }
    }
    return false;
}

float animation_controller::get_current_value(uint32_t anim_id) const {
    auto it = m_animations.find(anim_id);
    if (it != m_animations.end()) {
        return it->second.to_value;
    }
    return 0.0f;
}

void animation_controller::clear_animations_for_element(const std::wstring& element_id) {
    std::vector<uint32_t> to_remove;
    for (auto& [id, anim] : m_animations) {
        if (anim.element_id == element_id) {
            to_remove.push_back(id);
        }
    }
    for (auto id : to_remove) {
        m_animations.erase(id);
    }
}

float animation_controller::apply_easing(float t, const std::wstring& easing) {
    if (easing == L"linear") {
        return t;
    }
    if (easing == L"ease") {
        return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
    }
    if (easing == L"ease-in") {
        return t * t;
    }
    if (easing == L"ease-out") {
        return 1 - (1 - t) * (1 - t);
    }
    if (easing == L"ease-in-out") {
        return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
    }
    if (easing == L"cubic-bezier") {
        return t;
    }
    return t;
}

} // namespace hre::render