#pragma once

#include <functional>
#include <chrono>
#include <vector>
#include <memory>

namespace hyperion::ui {

enum class easing_type {
    LINEAR,
    EASE_IN_OUT_QUAD,
    EASE_OUT_QUART,
    EASE_OUT_ELASTIC,
    EASE_OUT_BOUNCE,
    EASE_IN_BACK,
    EASE_IN_OUT_CUBIC,
    EASE_OUT_EXPO
};

float apply_easing(easing_type type, float t);

class animatable_value {
public:
    virtual ~animatable_value() = default;
    virtual void set(float value) = 0;
    virtual float get() const = 0;
};

struct animation_handle {
    int id = 0;
    bool valid = false;
};

class animation_controller {
public:
    static animation_controller& instance() {
        static animation_controller inst;
        return inst;
    }

    animation_controller();
    ~animation_controller();

    animation_handle animate(
        float from, float to,
        std::chrono::milliseconds duration,
        easing_type curve,
        std::shared_ptr<animatable_value> target,
        std::function<void()> on_complete = nullptr
    );

    bool tick();
    void cancel(animation_handle handle);
    void cancel_all();

    int active_count() const { return (int)m_animations.size(); }

private:
    struct animation {
        int id;
        float from;
        float to;
        float current;
        std::chrono::milliseconds duration;
        easing_type curve;
        std::chrono::steady_clock::time_point start_time;
        std::shared_ptr<animatable_value> target;
        std::function<void()> on_complete;
        bool cancelled = false;
    };

    std::vector<std::unique_ptr<animation>> m_animations;
    int m_next_id = 1;
};

} // namespace hyperion::ui
