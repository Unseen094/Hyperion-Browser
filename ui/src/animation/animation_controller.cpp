#include <hyperion/ui/animation/animation_controller.hpp>
#include <hyperion/platform/logging.hpp>
#include <cmath>

namespace hyperion::ui {

float apply_easing(easing_type type, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    switch (type) {
        case easing_type::LINEAR:
            return t;
        case easing_type::EASE_IN_OUT_QUAD:
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        case easing_type::EASE_OUT_QUART:
            return 1.0f - std::pow(1.0f - t, 4.0f);
        case easing_type::EASE_OUT_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            float c4 = 2.0f * 3.14159f / 3.0f;
            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }
        case easing_type::EASE_OUT_BOUNCE: {
            if (t < 1.0f / 2.75f) return 7.5625f * t * t;
            if (t < 2.0f / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
            if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
            t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f;
        }
        case easing_type::EASE_IN_BACK:
            return 2.70158f * t * t * t - 1.70158f * t * t;
        case easing_type::EASE_IN_OUT_CUBIC:
            return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        case easing_type::EASE_OUT_EXPO:
            return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    }
    return t;
}

animation_controller::animation_controller() = default;
animation_controller::~animation_controller() = default;

animation_handle animation_controller::animate(
    float from, float to,
    std::chrono::milliseconds duration,
    easing_type curve,
    std::shared_ptr<animatable_value> target,
    std::function<void()> on_complete
) {
    auto anim = std::make_unique<animation>();
    anim->id = m_next_id++;
    anim->from = from;
    anim->to = to;
    anim->current = from;
    anim->duration = duration;
    anim->curve = curve;
    anim->start_time = std::chrono::steady_clock::now();
    anim->target = std::move(target);
    anim->on_complete = std::move(on_complete);

    animation_handle handle;
    handle.id = anim->id;
    handle.valid = true;

    m_animations.push_back(std::move(anim));
    return handle;
}

bool animation_controller::tick() {
    if (m_animations.empty()) return false;

    auto now = std::chrono::steady_clock::now();
    bool any_active = false;

    for (auto& anim : m_animations) {
        if (anim->cancelled) continue;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - anim->start_time);
        float t = std::min(1.0f, (float)elapsed.count() / (float)anim->duration.count());
        float eased = apply_easing(anim->curve, t);
        anim->current = anim->from + (anim->to - anim->from) * eased;

        if (anim->target) {
            anim->target->set(anim->current);
        }

        if (t >= 1.0f) {
            if (anim->target) anim->target->set(anim->to);
            if (anim->on_complete) anim->on_complete();
            anim->cancelled = true;
        } else {
            any_active = true;
        }
    }

    m_animations.erase(
        std::remove_if(m_animations.begin(), m_animations.end(),
            [](const auto& a) { return a->cancelled; }),
        m_animations.end()
    );

    return any_active;
}

void animation_controller::cancel(animation_handle handle) {
    for (auto& anim : m_animations) {
        if (anim->id == handle.id) {
            anim->cancelled = true;
            break;
        }
    }
}

void animation_controller::cancel_all() {
    for (auto& anim : m_animations) {
        anim->cancelled = true;
    }
    m_animations.clear();
}

} // namespace hyperion::ui
