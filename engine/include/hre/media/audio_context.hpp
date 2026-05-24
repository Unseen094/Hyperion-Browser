#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace hre::media {

enum class audio_node_type {
    SOURCE,
    GAIN,
    DELAY,
    BANDPASS,
    LOWPASS,
    HIGHPASS,
    CONVOLVER,
    ANALYSER,
    WAVE_SHAPER,
    STEREO_PANNER,
    DYNAMICS_COMPRESSOR,
    OSCILLATOR,
    DESTINATION
};

struct audio_buffer {
    int channels = 2;
    int sample_rate = 48000;
    int frames = 0;
    std::vector<std::vector<float>> channel_data; // [channel][sample]

    audio_buffer() = default;

    audio_buffer(int ch, int sr, int frames_count)
        : channels(ch), sample_rate(sr), frames(frames_count) {
        channel_data.resize(ch);
        for (auto& cd : channel_data) cd.resize(frames_count, 0.0f);
    }

    float get_sample(int channel, int frame) const {
        if (channel < 0 || channel >= channels || frame < 0 || frame >= frames) return 0;
        return channel_data[channel][frame];
    }

    void set_sample(int channel, int frame, float value) {
        if (channel >= 0 && channel < channels && frame >= 0 && frame < frames)
            channel_data[channel][frame] = value;
    }

    void fill(float value) {
        for (auto& cd : channel_data) std::fill(cd.begin(), cd.end(), value);
    }

    float duration_seconds() const { return (float)frames / (float)sample_rate; }

    static audio_buffer from_interleaved(const float* interleaved, int channels, int sample_rate, int frames);
    std::vector<float> to_interleaved() const;

    void mix(const audio_buffer& other, float gain = 1.0f);
    void apply_gain(float gain);
};

// ---- Audio Node Base ------------------------------------------------------

class audio_node {
public:
    audio_node() = default;
    virtual ~audio_node() = default;

    virtual audio_node_type type() const = 0;
    virtual void process(const audio_buffer& input, audio_buffer& output, int frames) = 0;
    virtual void reset() {}

    void connect(std::shared_ptr<audio_node> destination) {
        m_outputs.push_back(destination);
    }

    void disconnect(std::shared_ptr<audio_node> destination) {
        for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
            if (it->lock() == destination) { m_outputs.erase(it); break; }
        }
    }

    void disconnect_all() { m_outputs.clear(); }

    const std::vector<std::weak_ptr<audio_node>>& outputs() const { return m_outputs; }
    int num_inputs() const { return m_num_inputs; }
    int num_outputs() const { return m_num_outputs; }

protected:
    std::vector<std::weak_ptr<audio_node>> m_outputs;
    int m_num_inputs = 1;
    int m_num_outputs = 1;
};

// ---- Audio Node Implementations -------------------------------------------

class gain_node : public audio_node {
public:
    gain_node() : m_gain(1.0f) {}
    audio_node_type type() const override { return audio_node_type::GAIN; }
    void set_gain(float g) { m_gain = g; }
    float gain() const { return m_gain; }
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;

private:
    float m_gain;
};

class oscillator_node : public audio_node {
public:
    enum wave_type { SINE, SQUARE, SAWTOOTH, TRIANGLE, CUSTOM };

    oscillator_node(float sample_rate = 48000)
        : m_sample_rate(sample_rate), m_type(SINE), m_frequency(440.0f) {}

    audio_node_type type() const override { return audio_node_type::OSCILLATOR; }
    void set_type(wave_type t) { m_type = t; }
    void set_frequency(float freq) { m_frequency = freq; }
    void set_detune(float cents) { m_detune = cents; }

    void start(double when = 0);
    void stop(double when = 0);

    void process(const audio_buffer& input, audio_buffer& output, int frames) override;
    void reset() override { m_phase = 0.0f; }

private:
    float generate_sample(float phase, wave_type type) const;

    float m_sample_rate;
    wave_type m_type;
    float m_frequency;
    float m_detune = 0;
    float m_phase = 0;
    bool m_running = false;
};

class biquad_filter_node : public audio_node {
public:
    enum filter_type { LOWPASS, HIGHPASS, BANDPASS, NOTCH, ALLPASS, PEAKING, LOWSHELF, HIGHSHELF };

    biquad_filter_node() : m_type(LOWPASS), m_frequency(350.0f), m_q(1.0f), m_gain(0.0f) {}

    audio_node_type type() const override { return m_type == LOWPASS ? audio_node_type::LOWPASS : audio_node_type::BANDPASS; }
    void set_type(filter_type t) { m_type = t; update_coefficients(); }
    void set_frequency(float freq) { m_frequency = freq; update_coefficients(); }
    void set_q(float q) { m_q = q; update_coefficients(); }
    void set_gain(float g) { m_gain = g; update_coefficients(); }

    void process(const audio_buffer& input, audio_buffer& output, int frames) override;
    void reset() override;

private:
    void update_coefficients();
    float process_sample(float sample, int channel);

    filter_type m_type;
    float m_frequency;
    float m_q;
    float m_gain;
    float m_sample_rate = 48000;

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::vector<float> x1, x2, y1, y2; // per-channel state
};

class delay_node : public audio_node {
public:
    delay_node(float max_delay_ms = 1000.0f, float sample_rate = 48000)
        : m_max_samples((int)(max_delay_ms * sample_rate / 1000.0f))
        , m_sample_rate(sample_rate)
        , m_delay_time(0.5f) {
        m_buffer.resize(m_max_samples, 0.0f);
    }

    audio_node_type type() const override { return audio_node_type::DELAY; }
    void set_delay_time(float seconds) { m_delay_time = std::min(seconds, m_max_samples / m_sample_rate); }
    float delay_time() const { return m_delay_time; }

    void process(const audio_buffer& input, audio_buffer& output, int frames) override;
    void reset() override { std::fill(m_buffer.begin(), m_buffer.end(), 0.0f); m_write_pos = 0; }

private:
    int m_max_samples;
    float m_sample_rate;
    float m_delay_time;
    std::vector<float> m_buffer;
    int m_write_pos = 0;
};

class analyser_node : public audio_node {
public:
    analyser_node(int fft_size = 2048) : m_fft_size(fft_size) {
        m_time_domain.resize(fft_size, 0.0f);
        m_frequency_domain.resize(fft_size / 2, 0.0f);
    }

    audio_node_type type() const override { return audio_node_type::ANALYSER; }
    void process(const audio_buffer& input, audio_buffer& output, int frames) override;

    const std::vector<float>& get_time_domain() const { return m_time_domain; }
    const std::vector<float>& get_frequency_domain() const { return m_frequency_domain; }
    float get_byte_frequency_data(int bin) const;

    void set_fft_size(int size) { m_fft_size = size; m_time_domain.resize(size); m_frequency_domain.resize(size / 2); }
    int fft_size() const { return m_fft_size; }
    int frequency_bin_count() const { return m_fft_size / 2; }

private:
    void compute_fft();

    int m_fft_size;
    int m_write_cursor = 0;
    std::vector<float> m_time_domain;
    std::vector<float> m_frequency_domain;
    float m_sample_rate = 48000;
};

// ---- AudioContext ---------------------------------------------------------

class audio_context {
public:
    audio_context();
    ~audio_context();

    bool init();
    void close();
    bool is_running() const { return m_running; }

    float sample_rate() const { return m_sample_rate; }
    int channel_count() const { return m_channel_count; }
    double current_time() const { return m_current_time; }

    std::shared_ptr<gain_node> create_gain();
    std::shared_ptr<oscillator_node> create_oscillator();
    std::shared_ptr<biquad_filter_node> create_biquad_filter();
    std::shared_ptr<delay_node> create_delay(float max_delay_ms = 1000.0f);
    std::shared_ptr<analyser_node> create_analyser(int fft_size = 2048);

    void connect_final_node(std::shared_ptr<audio_node> node);

    void process_audio(int frames, float* output);

    using audio_callback = std::function<void(float* output, int frames, int channels)>;
    void set_render_callback(audio_callback cb) { m_render_callback = cb; }

    static audio_context* create_platform_context();

private:
    void render(float* output, int frames);

    std::vector<std::shared_ptr<audio_node>> m_nodes;
    audio_buffer m_output_buffer;
    bool m_running = false;
    float m_sample_rate = 48000;
    int m_channel_count = 2;
    double m_current_time = 0;
    audio_callback m_render_callback;
};

// ---- Audio Utility Functions ----------------------------------------------

namespace audio_utils {
    void interleave(const std::vector<std::vector<float>>& channel_data, float* output, int frames);
    void deinterleave(const float* input, std::vector<std::vector<float>>& channel_data, int channels, int frames);
    void apply_window(float* data, int size, int window_type = 0);
    void compute_fft(const float* input, float* output_re, float* output_im, int size);
    float db_to_linear(float db);
    float linear_to_db(float linear);
    float midi_to_frequency(int midi_note);
    int frequency_to_midi(float frequency);
}

} // namespace hre::media
