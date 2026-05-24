#define _USE_MATH_DEFINES
#include "hre/layout/animation_controller.hpp"
#include "hre/layout/layout_engine.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace hre::layout {

void AnimationController::register_keyframes(const std::string& name, const std::vector<Keyframe>& keyframes) {
    KeyframeAnimation anim;
    anim.name = name;
    anim.keyframes = keyframes;

    // Sort by offset
    std::sort(anim.keyframes.begin(), anim.keyframes.end(), [](const Keyframe& a, const Keyframe& b) {
        return a.offset < b.offset;
    });

    animations_[name] = anim;
}

void AnimationController::start_animation(const std::shared_ptr<LayoutNode>& target, const std::string& name) {
    auto it = animations_.find(name);
    if (it == animations_.end()) return;

    ActiveAnimation active;
    active.definition = &it->second;
    active.target = target;
    active.elapsed = 0;
    active.progress = 0;
    active.iterations_done = 0;
    active.start_time = std::chrono::steady_clock::now();
    active_animations_.push_back(active);
}

void AnimationController::start_transition(const std::shared_ptr<LayoutNode>& target, const std::string& property,
                                           const css::CssValue& from, const css::CssValue& to,
                                           double duration, const std::wstring& timing, double delay) {
    Transition trans;
    trans.property = property;
    trans.duration = duration;
    trans.timing_function = timing;
    trans.delay = delay;
    trans.from_value = from;
    trans.to_value = to;
    trans.elapsed = 0;
    trans.active = true;

    // Replace existing transition for same property
    for (auto& t : active_transitions_) {
        if (t.first == target && t.second.property == property) {
            t.second = trans;
            return;
        }
    }

    active_transitions_.emplace_back(target, trans);
}

double AnimationController::apply_timing_function(double t, const std::wstring& func) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;

    if (func == L"linear") return t;
    if (func == L"ease-in") return t * t;
    if (func == L"ease-out") return t * (2 - t);
    if (func == L"ease-in-out") return ease_in_out(t);
    if (func.find(L"cubic-bezier") == 0) {
        // Parse cubic-bezier and evaluate
        // Simplified: default to ease
        return ease_in_out(t);
    }
    // Default ease
    return ease_in_out(t);
}

double AnimationController::ease_in_out(double t) {
    return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

double AnimationController::interpolate(double from, double to, double t) {
    return from + (to - from) * t;
}

void AnimationController::apply_animation_frame(const ActiveAnimation& anim) {
    if (!anim.target || !anim.definition || anim.definition->keyframes.empty()) return;

    double p = anim.progress;
    const auto& kfs = anim.definition->keyframes;

    // Find surrounding keyframes
    size_t from_idx = 0;
    size_t to_idx = kfs.size() - 1;
    for (size_t i = 0; i + 1 < kfs.size(); ++i) {
        if (p >= kfs[i].offset && p <= kfs[i + 1].offset) {
            from_idx = i;
            to_idx = i + 1;
            break;
        }
    }

    const auto& from_kf = kfs[from_idx];
    const auto& to_kf = kfs[to_idx];

    double local_t = (from_kf.offset == to_kf.offset) ? 0 :
                     (p - from_kf.offset) / (to_kf.offset - from_kf.offset);
    local_t = apply_timing_function(local_t, anim.definition->timing_function);

    // Apply interpolated keyframe values to target style
    for (const auto& [prop, from_val] : from_kf.properties) {
        auto to_it = to_kf.properties.find(prop);
        if (to_it == to_kf.properties.end()) continue;

        const auto& to_val = to_it->second;

        // Interpolate based on unit type
        if (from_val.unit == to_val.unit && from_val.unit == css::CssUnit::Pixel) {
            double interpolated = interpolate(from_val.number, to_val.number, local_t);
            if (prop == "opacity") {
                anim.target->style.opacity = css::CssValue{interpolated, css::CssUnit::None, L""};
            } else if (prop == "transform") {
                // Simplified transform interpolation
                anim.target->style.transform = (interpolated != 0) ? L"translate(" + std::to_wstring(interpolated) + L"px)" : L"none";
            }
        }
    }
}

void AnimationController::apply_transition_frame(const Transition& trans, std::shared_ptr<LayoutNode>& target) {
    if (!target) return;

    double p = trans.duration > 0 ? std::min(1.0, trans.elapsed / trans.duration) : 1.0;
    p = apply_timing_function(p, trans.timing_function);

    if (trans.from_value.unit == trans.to_value.unit && trans.from_value.unit == css::CssUnit::Pixel) {
        double val = interpolate(trans.from_value.number, trans.to_value.number, p);

        if (trans.property == "opacity") {
            target->style.opacity = css::CssValue{val, css::CssUnit::None, L""};
        } else if (trans.property == "width") {
            target->content_box.width = val;
        } else if (trans.property == "height") {
            target->content_box.height = val;
        } else if (trans.property == "left") {
            target->content_box.x = val;
        } else if (trans.property == "top") {
            target->content_box.y = val;
        } else if (trans.property == "transform") {
            target->style.transform = L"translateX(" + std::to_wstring(val) + L"px)";
        }
    }
}

void AnimationController::tick(double delta_time) {
    // Update animations
    for (auto it = active_animations_.begin(); it != active_animations_.end();) {
        auto& anim = *it;
        if (!anim.definition || !anim.target) {
            it = active_animations_.erase(it);
            continue;
        }

        anim.elapsed += delta_time;
        if (anim.elapsed < anim.definition->delay) {
            ++it;
            continue;
        }

        double active_time = anim.elapsed - anim.definition->delay;
        double dur = anim.definition->duration;
        anim.progress = dur > 0 ? std::fmod(active_time, dur) / dur : 1.0;

        if (active_time >= dur * anim.definition->iteration_count &&
            anim.definition->iteration_count > 0) {
            anim.progress = 1.0;
            apply_animation_frame(anim);
            it = active_animations_.erase(it);
            continue;
        }

        // Handle direction
        int current_iteration = static_cast<int>(active_time / dur);
        if (anim.definition->direction == L"alternate" && current_iteration % 2 == 1) {
            anim.progress = 1.0 - anim.progress;
        } else if (anim.definition->direction == L"reverse") {
            anim.progress = 1.0 - anim.progress;
        } else if (anim.definition->direction == L"alternate-reverse") {
            if (current_iteration % 2 == 0) anim.progress = 1.0 - anim.progress;
        }

        apply_animation_frame(anim);
        ++it;
    }

    // Update transitions
    for (auto it = active_transitions_.begin(); it != active_transitions_.end();) {
        auto& [target, trans] = *it;
        trans.elapsed += delta_time;

        if (trans.elapsed < trans.delay) {
            ++it;
            continue;
        }

        double active_time = trans.elapsed - trans.delay;
        if (active_time >= trans.duration) {
            trans.elapsed = trans.duration;
            apply_transition_frame(trans, target);
            it = active_transitions_.erase(it);
            continue;
        }

        apply_transition_frame(trans, target);
        ++it;
    }
}

bool AnimationController::has_active_animations() const {
    return !active_animations_.empty() || !active_transitions_.empty();
}

css::CssValue AnimationController::apply_filter(const std::string& filter_type, const css::CssValue& input, double parameter) {
    if (filter_type == "blur") {
        // Blur would use convolution kernel - simplified as value passthrough
        return input;
    } else if (filter_type == "brightness") {
        if (input.unit == css::CssUnit::Pixel || input.unit == css::CssUnit::None) {
            return css::CssValue{input.number * parameter, css::CssUnit::Pixel};
        }
    } else if (filter_type == "contrast") {
        if (input.unit == css::CssUnit::Pixel || input.unit == css::CssUnit::None) {
            double mid = 128;
            double val = (input.number - mid) * parameter + mid;
            return css::CssValue{val, css::CssUnit::Pixel};
        }
    } else if (filter_type == "grayscale") {
        if (input.unit == css::CssUnit::Pixel || input.unit == css::CssUnit::None) {
            double avg = input.number;
            return css::CssValue{avg * (1 - parameter) + parameter * avg, css::CssUnit::Pixel};
        }
    } else if (filter_type == "saturate") {
        return input;
    } else if (filter_type == "sepia") {
        return input;
    } else if (filter_type == "hue-rotate") {
        return input;
    } else if (filter_type == "invert") {
        if (input.unit == css::CssUnit::Pixel || input.unit == css::CssUnit::None) {
            return css::CssValue{255 - input.number, css::CssUnit::Pixel};
        }
    } else if (filter_type == "drop-shadow") {
        return input; // Simplified
    }

    return input;
}

} // namespace hre::layout
