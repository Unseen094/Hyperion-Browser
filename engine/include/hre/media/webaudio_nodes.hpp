#pragma once

#include "hre/media/audio_context.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <complex>
#include <mutex>

namespace hre::media {

class audio_buffer_source_node : public audio_node {
public:
    audio_buffer_source_node(float sample_rate = 48000);

    audio_node_type type() const override { return audio_node_type::SOURCE; }
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;
    void reset() override;

    void set_buffer(std::shared_ptr<audio_buffer> buffer);
    void start(double when = 0, double offset = 0, double duration = 0);
    void stop(double when = 0);

    bool loop() const { return loop_; }
    void set_loop(bool l) { loop_ = l; }
    double loop_start() const { return loop_start_; }
    void set_loop_start(double s) { loop_start_ = s; }
    double loop_end() const { return loop_end_; }
    void set_loop_end(double e) { loop_end_ = e; }

    bool playing() const { return playing_; }

private:
    std::shared_ptr<audio_buffer> buffer_;
    double playback_position_ = 0;
    int start_frame_ = 0;
    int end_frame_ = 0;
    bool playing_ = false;
    bool loop_ = false;
    double loop_start_ = 0;
    double loop_end_ = 0;
    float sample_rate_;
};

class biquad_filter_node_ext : public audio_node {
public:
    enum filter_type { LOWPASS, HIGHPASS, BANDPASS, NOTCH, ALLPASS, PEAKING, LOWSHELF, HIGHSHELF };

    biquad_filter_node_ext();

    audio_node_type type() const override;
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;
    void reset() override;

    void set_type(filter_type t) { type_ = t; update_coefficients(); }
    filter_type get_type() const { return type_; }
    void set_frequency(float f) { frequency_ = f; update_coefficients(); }
    float frequency() const { return frequency_; }
    void set_q(float q) { q_ = q; update_coefficients(); }
    float q() const { return q_; }
    void set_gain(float g) { gain_ = g; update_coefficients(); }
    float gain() const { return gain_; }

private:
    void update_coefficients();
    float process_sample(float sample, int ch);

    filter_type type_ = LOWPASS;
    float frequency_ = 350.0f;
    float q_ = 1.0f;
    float gain_ = 0.0f;
    float sample_rate_ = 48000;

    float b0_ = 1, b1_ = 0, b2_ = 0, a0_ = 1, a1_ = 0, a2_ = 0;
    struct channel_state { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    std::vector<channel_state> state_;
};

class wave_shaper_node : public audio_node {
public:
    wave_shaper_node();

    audio_node_type type() const override { return audio_node_type::WAVE_SHAPER; }
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;

    void set_curve(const std::vector<float>& curve);
    const std::vector<float>& curve() const { return curve_; }
    void set_oversample(int o) { oversample_ = o; }
    int oversample() const { return oversample_; }

private:
    float shape(float x) const;

    std::vector<float> curve_;
    int oversample_ = 0;
};

class audio_destination_node : public audio_node {
public:
    audio_destination_node();
    ~audio_destination_node();

    audio_node_type type() const override { return audio_node_type::DESTINATION; }
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;

    bool init_wasapi();
    void close_wasapi();
    bool is_initialized() const { return wasapi_initialized_; }

private:
    bool wasapi_initialized_ = false;
    void* wasapi_device_ = nullptr;
    void* wasapi_client_ = nullptr;
    void* wasapi_render_ = nullptr;
};

class convolver_node : public audio_node {
public:
    convolver_node();

    audio_node_type type() const override { return audio_node_type::CONVOLVER; }
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;
    void reset() override;

    void set_buffer(std::shared_ptr<audio_buffer> impulse);
    std::shared_ptr<audio_buffer> buffer() const { return impulse_; }
    void set_normalize(bool n) { normalize_ = n; }
    bool normalize() const { return normalize_; }

private:
    void compute_fft_convolve(const std::vector<float>& signal, const std::vector<float>& kernel,
                              std::vector<float>& result);
    void compute_fft(const float* input, size_t n, bool inverse,
                     std::vector<std::complex<float>>& out);
    void build_impulse_fft();

    std::shared_ptr<audio_buffer> impulse_;
    bool normalize_ = true;
    std::vector<std::vector<std::complex<float>>> impulse_fft_;
    int fft_size_ = 2048;
    std::vector<float> prev_input_;
};

class gain_node_ext : public audio_node {
public:
    gain_node_ext() : gain_(1.0f) {}

    audio_node_type type() const override { return audio_node_type::GAIN; }
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;

    void set_gain(float g) { gain_ = g; }
    float get_gain() const { return gain_; }

private:
    float gain_;
};

} // namespace hre::media
