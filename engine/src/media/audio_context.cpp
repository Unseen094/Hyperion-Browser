#include <hre/media/audio_context.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace hre::media {

// ---- audio_buffer ---------------------------------------------------------

audio_buffer audio_buffer::from_interleaved(const float* interleaved, int channels, int sample_rate, int frames) {
    audio_buffer buf(channels, sample_rate, frames);
    for (int ch = 0; ch < channels; ++ch) {
        for (int f = 0; f < frames; ++f) {
            buf.channel_data[ch][f] = interleaved[f * channels + ch];
        }
    }
    return buf;
}

std::vector<float> audio_buffer::to_interleaved() const {
    std::vector<float> result(frames * channels);
    for (int f = 0; f < frames; ++f) {
        for (int ch = 0; ch < channels; ++ch) {
            result[f * channels + ch] = get_sample(ch, f);
        }
    }
    return result;
}

void audio_buffer::mix(const audio_buffer& other, float gain) {
    int min_frames = std::min(frames, other.frames);
    int min_ch = std::min(channels, other.channels);
    for (int ch = 0; ch < min_ch; ++ch) {
        for (int f = 0; f < min_frames; ++f) {
            channel_data[ch][f] += other.channel_data[ch][f] * gain;
        }
    }
}

void audio_buffer::apply_gain(float gain) {
    for (auto& cd : channel_data) {
        for (auto& s : cd) s *= gain;
    }
}

// ---- gain_node ------------------------------------------------------------

void gain_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    int channels = std::min(input.channels, output.channels);
    for (int ch = 0; ch < channels; ++ch) {
        for (int f = 0; f < frames; ++f) {
            output.channel_data[ch][f] = input.channel_data[ch][f] * m_gain;
        }
    }
}

// ---- oscillator_node ------------------------------------------------------

void oscillator_node::start(double when) { (void)when; m_running = true; }
void oscillator_node::stop(double when) { (void)when; m_running = false; }

float oscillator_node::generate_sample(float phase, wave_type type) const {
    switch (type) {
        case SINE:
            return std::sin(phase * 2.0f * static_cast<float>(std::numbers::pi));
        case SQUARE:
            return (std::sin(phase * 2.0f * static_cast<float>(std::numbers::pi)) >= 0) ? 1.0f : -1.0f;
        case SAWTOOTH:
            return 2.0f * (phase - std::floor(phase + 0.5f));
        case TRIANGLE:
            return 4.0f * std::abs(phase - std::floor(phase + 0.5f)) - 1.0f;
        default:
            return 0.0f;
    }
}

void oscillator_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    (void)input;
    if (!m_running) {
        for (int ch = 0; ch < output.channels; ++ch) {
            for (int f = 0; f < frames; ++f) output.channel_data[ch][f] = 0;
        }
        return;
    }

    float freq = m_frequency * std::pow(2.0f, m_detune / 1200.0f);
    float phase_inc = freq / m_sample_rate;

    for (int f = 0; f < frames; ++f) {
        float sample = generate_sample(m_phase, m_type);
        for (int ch = 0; ch < output.channels; ++ch) {
            output.channel_data[ch][f] = sample;
        }
        m_phase += phase_inc;
        if (m_phase >= 1.0f) m_phase -= 1.0f;
    }
}

// ---- biquad_filter_node ---------------------------------------------------

void biquad_filter_node::update_coefficients() {
    float w0 = 2.0f * static_cast<float>(std::numbers::pi) * m_frequency / m_sample_rate;
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * m_q);
    float A = std::pow(10.0f, m_gain / 40.0f);

    switch (m_type) {
        case LOWPASS:
            b0 = (1.0f - cos_w0) / 2.0f;
            b1 = 1.0f - cos_w0;
            b2 = (1.0f - cos_w0) / 2.0f;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;
        case HIGHPASS:
            b0 = (1.0f + cos_w0) / 2.0f;
            b1 = -(1.0f + cos_w0);
            b2 = (1.0f + cos_w0) / 2.0f;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;
        case BANDPASS:
            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;
        default:
            break;
    }

    float a0_norm = 1.0f + alpha;
    b0 /= a0_norm;
    b1 /= a0_norm;
    b2 /= a0_norm;
    a1 /= a0_norm;
    a2 /= a0_norm;
}

float biquad_filter_node::process_sample(float sample, int channel) {
    float out = b0 * sample + b1 * x1[channel] + b2 * x2[channel] - a1 * y1[channel] - a2 * y2[channel];
    x2[channel] = x1[channel];
    x1[channel] = sample;
    y2[channel] = y1[channel];
    y1[channel] = out;
    return out;
}

void biquad_filter_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    int channels = std::min(input.channels, output.channels);
    if (x1.size() != static_cast<size_t>(channels)) {
        x1.resize(channels, 0); x2.resize(channels, 0);
        y1.resize(channels, 0); y2.resize(channels, 0);
    }
    for (int ch = 0; ch < channels; ++ch) {
        for (int f = 0; f < frames; ++f) {
            output.channel_data[ch][f] = process_sample(input.channel_data[ch][f], ch);
        }
    }
}

void biquad_filter_node::reset() {
    std::fill(x1.begin(), x1.end(), 0);
    std::fill(x2.begin(), x2.end(), 0);
    std::fill(y1.begin(), y1.end(), 0);
    std::fill(y2.begin(), y2.end(), 0);
}

// ---- delay_node -----------------------------------------------------------

void delay_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    int delay_samples = static_cast<int>(m_delay_time * m_sample_rate);
    int channels = std::min(input.channels, output.channels);
    for (int ch = 0; ch < channels; ++ch) {
        for (int f = 0; f < frames; ++f) {
            int read_pos = (m_write_pos - delay_samples + m_max_samples) % m_max_samples;
            float delayed = m_buffer[read_pos];
            m_buffer[m_write_pos] = input.channel_data[ch][f];
            output.channel_data[ch][f] = input.channel_data[ch][f] + delayed * 0.5f;
            m_write_pos = (m_write_pos + 1) % m_max_samples;
        }
    }
}

// ---- analyser_node --------------------------------------------------------

void analyser_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    for (int f = 0; f < frames && m_write_cursor < m_fft_size; ++f) {
        m_time_domain[m_write_cursor++] = input.channel_data[0][f];
    }
    if (m_write_cursor >= m_fft_size) {
        compute_fft();
        m_write_cursor = 0;
    }
    output = input;
}

float analyser_node::get_byte_frequency_data(int bin) const {
    if (bin < 0 || bin >= static_cast<int>(m_frequency_domain.size())) return 0;
    return m_frequency_domain[bin];
}

void analyser_node::compute_fft() {
    std::vector<float> re(m_fft_size);
    std::vector<float> im(m_fft_size, 0);
    for (int i = 0; i < m_fft_size; ++i) {
        float window = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(std::numbers::pi) * i / (m_fft_size - 1)));
        re[i] = m_time_domain[i] * window;
    }

    int n = m_fft_size;
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * static_cast<float>(std::numbers::pi) / len;
        float wlen_re = std::cos(ang);
        float wlen_im = -std::sin(ang);
        for (int i = 0; i < n; i += len) {
            float w_re = 1, w_im = 0;
            for (int j = 0; j < len / 2; ++j) {
                float u_re = re[i + j], u_im = im[i + j];
                float v_re = re[i + j + len / 2] * w_re - im[i + j + len / 2] * w_im;
                float v_im = re[i + j + len / 2] * w_im + im[i + j + len / 2] * w_re;
                re[i + j] = u_re + v_re;
                im[i + j] = u_im + v_im;
                re[i + j + len / 2] = u_re - v_re;
                im[i + j + len / 2] = u_im - v_im;
                float t = w_re * wlen_re - w_im * wlen_im;
                w_im = w_re * wlen_im + w_im * wlen_re;
                w_re = t;
            }
        }
    }

    for (int i = 0; i < m_fft_size / 2; ++i) {
        m_frequency_domain[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]) / m_fft_size;
    }
}

// ---- AudioContext ---------------------------------------------------------

audio_context::audio_context() {
    m_output_buffer = audio_buffer(m_channel_count, static_cast<int>(m_sample_rate), 1024);
}

audio_context::~audio_context() {
    close();
}

bool audio_context::init() {
    m_running = true;
    return true;
}

void audio_context::close() {
    m_running = false;
    m_nodes.clear();
}

std::shared_ptr<gain_node> audio_context::create_gain() {
    auto node = std::make_shared<gain_node>();
    m_nodes.push_back(node);
    return node;
}

std::shared_ptr<oscillator_node> audio_context::create_oscillator() {
    auto node = std::make_shared<oscillator_node>(m_sample_rate);
    m_nodes.push_back(node);
    return node;
}

std::shared_ptr<biquad_filter_node> audio_context::create_biquad_filter() {
    auto node = std::make_shared<biquad_filter_node>();
    m_nodes.push_back(node);
    return node;
}

std::shared_ptr<delay_node> audio_context::create_delay(float max_delay_ms) {
    auto node = std::make_shared<delay_node>(max_delay_ms, m_sample_rate);
    m_nodes.push_back(node);
    return node;
}

std::shared_ptr<analyser_node> audio_context::create_analyser(int fft_size) {
    auto node = std::make_shared<analyser_node>(fft_size);
    m_nodes.push_back(node);
    return node;
}

void audio_context::connect_final_node(std::shared_ptr<audio_node> node) {
    (void)node;
}

void audio_context::process_audio(int frames, float* output) {
    render(output, frames);
}

audio_context* audio_context::create_platform_context() {
    return new audio_context();
}

void audio_context::render(float* output, int frames) {
    if (m_render_callback) {
        m_render_callback(output, frames, m_channel_count);
        return;
    }

    m_output_buffer = audio_buffer(m_channel_count, static_cast<int>(m_sample_rate), frames);
    m_output_buffer.fill(0);

    audio_buffer silence(m_channel_count, static_cast<int>(m_sample_rate), frames);
    silence.fill(0);

    for (auto& node : m_nodes) {
        audio_buffer temp(m_channel_count, static_cast<int>(m_sample_rate), frames);
        temp.fill(0);
        node->process(silence, temp, frames);
        m_output_buffer.mix(temp, 1.0f);
    }

    auto interleaved = m_output_buffer.to_interleaved();
    std::memcpy(output, interleaved.data(), frames * m_channel_count * sizeof(float));

    m_current_time += static_cast<double>(frames) / m_sample_rate;
}

// ---- Audio Utilities ------------------------------------------------------

namespace audio_utils {

void interleave(const std::vector<std::vector<float>>& channel_data, float* output, int frames) {
    int channels = static_cast<int>(channel_data.size());
    for (int f = 0; f < frames; ++f) {
        for (int ch = 0; ch < channels; ++ch) {
            output[f * channels + ch] = (ch < static_cast<int>(channel_data.size()) && f < static_cast<int>(channel_data[ch].size()))
                ? channel_data[ch][f] : 0;
        }
    }
}

void deinterleave(const float* input, std::vector<std::vector<float>>& channel_data, int channels, int frames) {
    channel_data.resize(channels);
    for (int ch = 0; ch < channels; ++ch) {
        channel_data[ch].resize(frames);
        for (int f = 0; f < frames; ++f) {
            channel_data[ch][f] = input[f * channels + ch];
        }
    }
}

void apply_window(float* data, int size, int window_type) {
    (void)window_type;
    for (int i = 0; i < size; ++i) {
        data[i] *= 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(std::numbers::pi) * i / (size - 1)));
    }
}

void compute_fft(const float* input, float* output_re, float* output_im, int size) {
    std::vector<float> re(size);
    std::vector<float> im(size, 0);
    for (int i = 0; i < size; ++i) re[i] = input[i];

    for (int i = 1, j = 0; i < size; ++i) {
        int bit = size >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
        }
    }

    for (int len = 2; len <= size; len <<= 1) {
        float ang = 2.0f * static_cast<float>(std::numbers::pi) / len;
        float wlen_re = std::cos(ang);
        float wlen_im = -std::sin(ang);
        for (int i = 0; i < size; i += len) {
            float w_re = 1, w_im = 0;
            for (int j = 0; j < len / 2; ++j) {
                float u_re = re[i + j], u_im = 0;
                float v_re = re[i + j + len / 2] * w_re;
                float v_im = re[i + j + len / 2] * w_im;
                re[i + j] = u_re + v_re;
                re[i + j + len / 2] = u_re - v_re;
                float t = w_re * wlen_re - w_im * wlen_im;
                w_im = w_re * wlen_im + w_im * wlen_re;
                w_re = t;
            }
        }
    }

    for (int i = 0; i < size; ++i) {
        output_re[i] = re[i];
        output_im[i] = 0;
    }
}

float db_to_linear(float db) { return std::pow(10.0f, db / 20.0f); }
float linear_to_db(float linear) { return 20.0f * std::log10(std::max(linear, 1e-10f)); }
float midi_to_frequency(int midi_note) { return 440.0f * std::pow(2.0f, (midi_note - 69) / 12.0f); }
int frequency_to_midi(float frequency) { return static_cast<int>(std::round(69 + 12 * std::log2(frequency / 440.0f))); }

} // namespace audio_utils

} // namespace hre::media
