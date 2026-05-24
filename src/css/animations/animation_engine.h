#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include <mutex>
#include <cmath>

namespace hyperion::css::animation {

// CSS animation property types
enum class AnimationType : uint8_t {
    OPACITY,
    TRANSFORM,
    BACKGROUND_COLOR,
    WIDTH,
    HEIGHT,
    TOP,
    LEFT,
    FILTER,
    ROTATION,
    SCALE,
    TRANSLATE
};

// Timing function types (easing functions)
class TimingFunction {
public:
    virtual ~TimingFunction() = default;
    virtual float evaluate(float t) const = 0;
    virtual std::string name() const = 0;
};

class LinearEasing : public TimingFunction {
public:
    float evaluate(float t) const override { return t; }
    std::string name() const override { return "linear"; }
};

class EaseOutFunction : public TimingFunction {
public:
    float evaluate(float t) const override {
        return 1 - std::pow(1 - t, 3);
    }
    std::string name() const override { return "ease-out"; }
};

class EaseInOutFunction : public TimingFunction {
public:
    float evaluate(float t) const override {
        if (t < 0.5f) {
            return 2 * t * t;
        }
        return 1 - std::pow(-2 * t + 2, 2) / 2;
    }
    std::string name() const override { return "ease-in-out"; }
};

class CubicBezierFunction : public TimingFunction {
public:
    CubicBezierFunction(float p1x, float p1y, float p2x, float p2y)
        : p1_(p1x, p1y), p2_(p2x, p2y) {}
    
    float evaluate(float t) const override {
        if (t == 0) return 0;
        if (t == 1) return 1;
        
        return sample_cubic_bezier(p1_.x, p1_.y, p2_.x, p2_.y, t);
    }
    
    std::string name() const override { return "cubic-bezier"; }
    
private:
    struct Point { float x, y; };
    Point p1_, p2_;
    
    float sample_cubic_bezier(float x1, float y1, float x2, float y2, float t) const {
        // CPU-efficient bezier evaluation (De Casteljau's algorithm)
        float t2 = t * t;
        float mt = 1 - t;
        float mt2 = mt * mt;
        
        return (
            3 * mt2 * t * y1 + 
            3 * mt * t2 * y2 + 
            t2 * t
        );
    }
};

// Keyframe collection for single animation property
struct KeyframeCollection {
    std::vector<float> times;
    std::vector<float> values;
    std::string timing_function;
    std::shared_ptr<TimingFunction> function;
    std::string property_name;
};

class AnimationState {
public:
    AnimationState(uint32_t element_id, const std::string& animation_name)
        : element_id_(element_id), animation_name_(animation_name), 
          start_time_(std::chrono::steady_clock::now()), direction_(1) {}
    
    void set_keyframes(const std::vector<float>& times, const std::vector<float>& values,
                     const std::string& timing_function) {
        keyframes_.times = times;
        keyframes_.values = values;
        keyframes_.property_name = "animation";
        keyframes_.timing_function = timing_function;
        
        if (timing_function == "linear") {
            keyframes_.function = std::make_shared<LinearEasing>();
        } else if (timing_function == "ease-out") {
            keyframes_.function = std::make_shared<EaseOutFunction>();
        } else if (timing_function == "ease-in-out") {
            keyframes_.function = std::make_shared<EaseInOutFunction>();
        } else {
            // Custom bezier curve
            keyframes_.function = std::make_shared<CubicBezierFunction>(0.25f, 0.1f, 0.25f, 1.0f);
        }
        
        duration_ = keyframes_.times.back();
        pause_end = std::chrono::steady_clock::now() + std::chrono::milliseconds(0);
        reverse_duration_ = duration_;
    }
    
    void set_direction(int direction) {
        direction_ = direction;
        pause_end = std::chrono::steady_clock::now() + std::chrono::milliseconds(0);
    }
    
    void set_iteration_count(float count) { iteration_count_ = count; }
    void set_fill_mode(const std::string& mode) { fill_mode_ = mode; }
    void set_delay(int delay_ms) { delay_ms_ = delay_ms; }
    
    float get_current_progress() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
        
        if (delay_ms_ > 0) {
            if (elapsed < delay_ms_) return 0.0f;
            elapsed -= delay_ms_;
        }
        
        // Remove paused time
        if (current_state_ == PAUSED) {
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - pause_start - std::chrono::milliseconds(delay_ms_)
            ).count();
        }
        
        if (iteration_count_ > 0 && iterations_completed_ >= iteration_count_) {
            // Animation iteration complete - apply fill mode
            if (fill_mode_ == "forwards") {
                return std::min(1.0f, static_cast<float>(duration_));
            }
            if (fill_mode_ == "backwards") {
                return 0.0f;
            }
            if (fill_mode_ == "both") {
                return direction_ > 0 ? std::min(1.0f, static_cast<float>(duration_)) : 0.0f;
            }
            return 0.0f;
        }
        
        float total_time = reverse_duration_ * iteration_count_ * 2;
        float local_time = static_cast<float>(elapsed % total_time);
        if (local_time < reverse_duration_) {
            // Forward animation
            if (iterations_completed_ == iteration_count_ - 1 && fill_mode_ == "forwards") {
                return std::min(1.0f, local_time / duration_);
            }
            return local_time / reverse_duration_;
        } else {
            // Reverse animation
            return (reverse_duration_ - (local_time - reverse_duration_)) / duration_;
        }
    }
    
    float get_current_value() {
        float progress = get_current_progress();
        if (!keyframes_.function) return 0.0f;
        
        float eased = keyframes_.function->evaluate(progress);
        
        // Find closest keyframes
        for (size_t i = 1; i < keyframes_.times.size(); ++i) {
            if (progress <= keyframes_.times[i]) {
                float t1 = keyframes_.times[i-1] / duration_;
                float p1 = keyframes_.values[i-1];
                float p2 = keyframes_.values[i];
                return p1 + (p2 - p1) * std::min(1.0f, std::max(0.0f, eased - t1) * duration_);
            }
        }
        return keyframes_.values.back();
    }
    
    void pause() {
        if (current_state_ == PAUSED || current_state_ == STOPPED) return;
        pause_start = std::chrono::steady_clock::now();
        current_state_ = PAUSED;
    }
    
    void resume() {
        if (current_state_ != PAUSED) return;
        auto paused_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - pause_start
        ).count();
        start_time_ += paused_time;
        current_state_ = RUNNING;
    }
    
    void stop() {
        current_state_ = STOPPED;
    }
    
    bool is_complete() const {
        if (iteration_count_ <= 0) return false; // Infinite animation
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
        
        float total_time = reverse_duration_ * iteration_count_ * 2;
        return elapsed >= total_time;
    }
    
    void set_play_state(const std::string& state) {
        if (state == "running") {
            current_state_ = RUNNING;
        } else if (state == "paused") {
            current_state_ = PAUSED;
        }
    }
    
    const std::string& animation_name() const { return animation_name_; }
    uint32_t element_id() const { return element_id_; }

private:
    enum State { RUNNING, PAUSED, STOPPED };
    State current_state_ = RUNNING;
    uint32_t element_id_;
    std::string animation_name_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point pause_start;
    std::chrono::steady_clock::time_point pause_end;
    int delay_ms_ = 0;
    float duration_ = 0;
    float reverse_duration_ = 0;
    float iteration_count_ = 1;
    int direction_ = 1; // 1 for normal, -1 for reverse
    int iterations_completed_ = 0;
    std::string fill_mode_ = "none";
    KeyframeCollection keyframes_;
};

class AnimationEngine {
public:
    // Apply keyframes to element
    void add_animation(uint32_t element_id, const std::string& animation_name,
                     const std::string& property, const std::vector<float>& times,
                     const std::vector<float>& values, const std::string& timing_function = "linear") {
        std::lock_guard<std::mutex> lock(mutex_);
        auto anim = std::make_shared<AnimationState>(element_id, animation_name);
        anim->set_keyframes(times, values, timing_function);
        running_animations_[animation_name + ":" + std::to_string(element_id)] = anim;
    }
    
    void set_animation_properties(uint32_t element_id, const std::string& animation_name,
                                int delay_ms = 0, float iterations = 1,
                                const std::string& fill_mode = "none",
                                int direction = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = animation_name + ":" + std::to_string(element_id);
        auto it = running_animations_.find(key);
        if (it != running_animations_.end()) {
            it->second->set_delay(delay_ms);
            it->second->set_iteration_count(iterations);
            it->second->set_fill_mode(fill_mode);
            it->second->set_direction(direction);
        }
    }
    
    // Get current animation value for property
    float get_animation_value(uint32_t element_id, const std::string& animation_name,
                             const std::string& property) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = animation_name + ":" + std::to_string(element_id);
        auto it = running_animations_.find(key);
        if (it != running_animations_.end() && it->second->current_state_ != AnimationState::STOPPED) {
            return it->second->get_current_value();
        }
        return 0.0f;
    }
    
    // Control animation state
    void pause_animation(const std::string& animation_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [key, anim] : running_animations_) {
            if (anim->animation_name().find(animation_name) != std::string::npos) {
                anim->pause();
            }
        }
    }
    
    void resume_animation(const std::string& animation_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [key, anim] : running_animations_) {
            if (anim->animation_name().find(animation_name) != std::string::npos) {
                anim->resume();
            }
        }
    }
    
    void set_play_state(uint32_t element_id, const std::string& state) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [key, anim] : running_animations_) {
            if (anim->element_id() == element_id) {
                anim->set_play_state(state);
            }
        }
    }
    
    // Update animations (call this periodically)
    void update_animations() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = running_animations_.begin(); it != running_animations_.end(); ) {
            auto& anim = it->second;
            if (anim->is_complete()) {
                it = running_animations_.erase(it);
            } else if (anim->current_state_ == AnimationState::STOPPED) {
                it = running_animations_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    size_t animation_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_animations_.size();
    }
    
    void remove_animation(uint32_t element_id, const std::string& animation_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        running_animations_.erase(animation_name + ":" + std::to_string(element_id));
    }

private:
    std::unordered_map<std::string, std::shared_ptr<AnimationState>> running_animations_;
    mutable std::mutex mutex_;
};

} // namespace hyperion::css::animation