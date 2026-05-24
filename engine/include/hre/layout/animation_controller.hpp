#pragma once

#include "hre/css/style_engine.hpp"
#include "hre/dom/node.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace hre::layout {

struct LayoutNode;

struct Keyframe {
    double offset = 0; // 0.0 to 1.0
    std::unordered_map<std::string, css::CssValue> properties;
};

struct KeyframeAnimation {
    std::string name;
    std::vector<Keyframe> keyframes;
    double duration = 1.0;
    std::wstring timing_function = L"ease";
    double delay = 0;
    int iteration_count = 1;
    std::wstring direction = L"normal";
    std::wstring fill_mode = L"none";
    bool running = true;
};

struct ActiveAnimation {
    KeyframeAnimation* definition = nullptr;
    std::shared_ptr<LayoutNode> target;
    double elapsed = 0;
    double progress = 0;
    int iterations_done = 0;
    std::chrono::steady_clock::time_point start_time;
};

struct Transition {
    std::string property;
    double duration = 0.3;
    std::wstring timing_function = L"ease";
    double delay = 0;
    css::CssValue from_value;
    css::CssValue to_value;
    double elapsed = 0;
    bool active = false;
};

class AnimationController {
public:
    AnimationController() = default;

    void register_keyframes(const std::string& name, const std::vector<Keyframe>& keyframes);

    void start_animation(const std::shared_ptr<LayoutNode>& target, const std::string& name);
    void start_transition(const std::shared_ptr<LayoutNode>& target, const std::string& property,
                          const css::CssValue& from, const css::CssValue& to,
                          double duration, const std::wstring& timing, double delay);

    void tick(double delta_time);
    void apply_animation_frame(const ActiveAnimation& anim);
    void apply_transition_frame(const Transition& trans, std::shared_ptr<LayoutNode>& target);

    bool has_active_animations() const;

    // CSS filter support
    css::CssValue apply_filter(const std::string& filter_type, const css::CssValue& input, double parameter);

private:
    std::unordered_map<std::string, KeyframeAnimation> animations_;
    std::vector<ActiveAnimation> active_animations_;
    std::vector<std::pair<std::shared_ptr<LayoutNode>, Transition>> active_transitions_;

    double interpolate(double from, double to, double t);
    double apply_timing_function(double t, const std::wstring& func);
    double ease_in_out(double t);
};

} // namespace hre::layout
