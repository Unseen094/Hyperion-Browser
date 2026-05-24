#include <hre/media/webaudio_nodes.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <complex>
#include <numbers>

namespace hre::media {

static constexpr float PI = 3.14159265358979323846f;

// ---- AudioBufferSourceNode -------------------------------------------------

audio_buffer_source_node::audio_buffer_source_node(float sample_rate)
    : sample_rate_(sample_rate) {}

void audio_buffer_source_node::set_buffer(std::shared_ptr<audio_buffer> buffer) {
    buffer_ = std::move(buffer);
}

void audio_buffer_source_node::start(double when, double offset, double duration) {
    playing_ = true;
    if (buffer_) {
        start_frame_ = static_cast<int>(offset * sample_rate_);
        end_frame_ = duration > 0 ? start_frame_ + static_cast<int>(duration * sample_rate_)
                                   : buffer_->frames;
        playback_position_ = static_cast<double>(start_frame_);
    }
}

void audio_buffer_source_node::stop(double when) {
    playing_ = false;
}

void audio_buffer_source_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    output.fill(0.0f);
    if (!playing_ || !buffer_) return;

    int out_channels = std::min(output.channels, buffer_->channels);

    for (int ch = 0; ch < out_channels; ++ch) {
        for (int i = 0; i < frames; ++i) {
            int pos = static_cast<int>(playback_position_) + i;
            if (pos >= end_frame_ || pos >= buffer_->frames) {
                if (loop_) {
                    int loop_start_sample = static_cast<int>(loop_start_ * sample_rate_);
                    int loop_end_sample = loop_end_ > 0 ? static_cast<int>(loop_end_ * sample_rate_)
                                                        : buffer_->frames;
                    if (pos >= loop_end_sample) {
                        playback_position_ = static_cast<double>(loop_start_sample) - frames;
                        pos = loop_start_sample + (pos - loop_end_sample) % (loop_end_sample - loop_start_sample);
                    }
                } else {
                    playing_ = false;
                    break;
                }
            }
            if (pos >= 0 && pos < buffer_->frames) {
                output.channel_data[ch][i] = buffer_->channel_data[ch][pos];
            }
        }
    }

    playback_position_ += frames;
}

void audio_buffer_source_node::reset() {
    playback_position_ = 0;
    playing_ = false;
}

// ---- BiquadFilterNodeExt ---------------------------------------------------

biquad_filter_node_ext::biquad_filter_node_ext() {
    update_coefficients();
}

audio_node_type biquad_filter_node_ext::type() const {
    switch (type_) {
        case LOWPASS: return audio_node_type::LOWPASS;
        case HIGHPASS: return audio_node_type::HIGHPASS;
        default: return audio_node_type::BANDPASS;
    }
}

void biquad_filter_node_ext::update_coefficients() {
    float w0 = 2.0f * PI * frequency_ / sample_rate_;
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * q_);
    float A = std::pow(10.0f, gain_ / 40.0f);

    switch (type_) {
        case LOWPASS:
            b0_ = (1.0f - cos_w0) / 2.0f;
            b1_ = 1.0f - cos_w0;
            b2_ = b0_;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_w0;
            a2_ = 1.0f - alpha;
            break;
        case HIGHPASS:
            b0_ = (1.0f + cos_w0) / 2.0f;
            b1_ = -(1.0f + cos_w0);
            b2_ = b0_;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_w0;
            a2_ = 1.0f - alpha;
            break;
        case BANDPASS:
            b0_ = alpha;
            b1_ = 0.0f;
            b2_ = -alpha;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_w0;
            a2_ = 1.0f - alpha;
            break;
        case NOTCH:
            b0_ = 1.0f;
            b1_ = -2.0f * cos_w0;
            b2_ = 1.0f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_w0;
            a2_ = 1.0f - alpha;
            break;
        case ALLPASS:
            b0_ = 1.0f - alpha;
            b1_ = -2.0f * cos_w0;
            b2_ = 1.0f + alpha;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_w0;
            a2_ = 1.0f - alpha;
            break;
        case PEAKING:
            b0_ = 1.0f + alpha * A;
            b1_ = -2.0f * cos_w0;
            b2_ = 1.0f - alpha * A;
            a0_ = 1.0f + alpha / A;
            a1_ = -2.0f * cos_w0;
            a2_ = 1.0f - alpha / A;
            break;
        case LOWSHELF:
            {
                float sqrtA = std::sqrt(A);
                float ap = alpha * sqrtA;
                float ia = 1.0f / sqrtA;
                b0_ = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + ap * 2.0f);
                b1_ = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                b2_ = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - ap * 2.0f);
                a0_ = (A + 1.0f) + (A - 1.0f) * cos_w0 + ap * 2.0f;
                a1_ = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                a2_ = (A + 1.0f) + (A - 1.0f) * cos_w0 - ap * 2.0f;
            }
            break;
        case HIGHSHELF:
            {
                float sqrtA = std::sqrt(A);
                float ap = alpha * sqrtA;
                b0_ = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + ap * 2.0f);
                b1_ = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                b2_ = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - ap * 2.0f);
                a0_ = (A + 1.0f) - (A - 1.0f) * cos_w0 + ap * 2.0f;
                a1_ = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                a2_ = (A + 1.0f) - (A - 1.0f) * cos_w0 - ap * 2.0f;
            }
            break;
    }

    if (std::abs(a0_) < 1e-10f) a0_ = 1.0f;
    float ia0 = 1.0f / a0_;
    b0_ *= ia0; b1_ *= ia0; b2_ *= ia0;
    a1_ *= ia0; a2_ *= ia0;

    state_.resize(4, channel_state{});
}

void biquad_filter_node_ext::reset() {
    for (auto& s : state_) s = channel_state{};
}

float biquad_filter_node_ext::process_sample(float sample, int ch) {
    if (ch >= static_cast<int>(state_.size())) return sample;
    auto& s = state_[ch];
    float y = b0_ * sample + b1_ * s.x1 + b2_ * s.x2 - a1_ * s.y1 - a2_ * s.y2;
    s.x2 = s.x1; s.x1 = sample;
    s.y2 = s.y1; s.y1 = y;
    return y;
}

void biquad_filter_node_ext::process(const audio_buffer& input, audio_buffer& output, int frames) {
    int ch = std::min(input.channels, output.channels);
    for (int c = 0; c < ch; ++c) {
        for (int i = 0; i < frames; ++i) {
            output.channel_data[c][i] = process_sample(input.channel_data[c][i], c);
        }
    }
}

// ---- WaveShaperNode --------------------------------------------------------

wave_shaper_node::wave_shaper_node() {
    curve_ = {0.0f};
}

void wave_shaper_node::set_curve(const std::vector<float>& curve) {
    curve_ = curve;
    if (curve_.empty()) curve_.push_back(0.0f);
}

float wave_shaper_node::shape(float x) const {
    if (curve_.size() == 1) return curve_[0];
    float t = (x + 1.0f) * 0.5f;
    t = std::clamp(t, 0.0f, 1.0f);
    float idx = t * (static_cast<float>(curve_.size()) - 1.0f);
    int i0 = static_cast<int>(idx);
    int i1 = std::min(i0 + 1, static_cast<int>(curve_.size()) - 1);
    float frac = idx - static_cast<float>(i0);
    return curve_[i0] * (1.0f - frac) + curve_[i1] * frac;
}

void wave_shaper_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    int ch = std::min(input.channels, output.channels);
    for (int c = 0; c < ch; ++c) {
        for (int i = 0; i < frames; ++i) {
            output.channel_data[c][i] = shape(input.channel_data[c][i]);
        }
    }
}

// ---- AudioDestinationNode (WASAPI) -----------------------------------------

audio_destination_node::audio_destination_node() {
    m_num_outputs = 0;
}

audio_destination_node::~audio_destination_node() {
    close_wasapi();
}

bool audio_destination_node::init_wasapi() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) return false;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) { enumerator->Release(); return false; }

    hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, &wasapi_client_);
    if (FAILED(hr)) { device->Release(); enumerator->Release(); return false; }

    WAVEFORMATEX* format = nullptr;
    IAudioClient* client = static_cast<IAudioClient*>(wasapi_client_);
    hr = client->GetMixFormat(&format);
    if (FAILED(hr)) { close_wasapi(); enumerator->Release(); return false; }

    hr = client->Initialize(SHARED, 0, 100000, 0, format, nullptr);
    if (FAILED(hr)) { CoTaskMemFree(format); close_wasapi(); enumerator->Release(); return false; }

    hr = client->GetService(IID_IAudioRenderClient, &wasapi_render_);
    if (FAILED(hr)) { CoTaskMemFree(format); close_wasapi(); enumerator->Release(); return false; }

    wasapi_initialized_ = true;
    wasapi_device_ = device;

    CoTaskMemFree(format);
    enumerator->Release();
    return true;
}

void audio_destination_node::close_wasapi() {
    if (wasapi_render_) {
        static_cast<IAudioRenderClient*>(wasapi_render_)->Release();
        wasapi_render_ = nullptr;
    }
    if (wasapi_client_) {
        static_cast<IAudioClient*>(wasapi_client_)->Stop();
        static_cast<IAudioClient*>(wasapi_client_)->Release();
        wasapi_client_ = nullptr;
    }
    if (wasapi_device_) {
        static_cast<IMMDevice*>(wasapi_device_)->Release();
        wasapi_device_ = nullptr;
    }
    wasapi_initialized_ = false;
    CoUninitialize();
}

void audio_destination_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    if (!wasapi_initialized_) {
        output.fill(0.0f);
        return;
    }

    IAudioRenderClient* render = static_cast<IAudioRenderClient*>(wasapi_render_);
    IAudioClient* client = static_cast<IAudioClient*>(wasapi_client_);

    BYTE* buffer = nullptr;
    HRESULT hr = render->GetBuffer(frames, &buffer);
    if (FAILED(hr)) return;

    float* dest = reinterpret_cast<float*>(buffer);
    int ch = std::min(input.channels, output.channels);
    for (int i = 0; i < frames; ++i) {
        for (int c = 0; c < ch; ++c) {
            dest[i * output.channels + c] = input.channel_data[c][i];
        }
        for (int c = ch; c < output.channels; ++c) {
            dest[i * output.channels + c] = 0.0f;
        }
    }

    render->ReleaseBuffer(frames, 0);
    memcpy(output.channel_data[0].data(), input.channel_data[0].data(),
           frames * sizeof(float) * input.channels);
}

// ---- ConvolverNode ---------------------------------------------------------

convolver_node::convolver_node() {
    m_num_inputs = 2;
}

void convolver_node::set_buffer(std::shared_ptr<audio_buffer> impulse) {
    impulse_ = std::move(impulse);
    if (normalize_ && impulse_) {
        float sum = 0.0f;
        for (auto& cd : impulse_->channel_data) {
            sum = std::accumulate(cd.begin(), cd.end(), 0.0f,
                                  [](float a, float b) { return a + std::abs(b); });
        }
        if (sum > 0.0f) {
            float scale = 1.0f / sum;
            for (auto& cd : impulse_->channel_data) {
                for (auto& s : cd) s *= scale;
            }
        }
    }
    if (impulse_) build_impulse_fft();
}

void convolver_node::build_impulse_fft() {
    if (!impulse_) return;
    fft_size_ = 1;
    while (fft_size_ < impulse_->frames * 2) fft_size_ *= 2;

    impulse_fft_.resize(impulse_->channels);
    for (int ch = 0; ch < impulse_->channels; ++ch) {
        std::vector<float> padded(fft_size_, 0.0f);
        memcpy(padded.data(), impulse_->channel_data[ch].data(),
               impulse_->frames * sizeof(float));
        compute_fft(padded.data(), fft_size_, false, impulse_fft_[ch]);
    }
}

void convolver_node::compute_fft(const float* input, size_t n, bool inverse,
                                  std::vector<std::complex<float>>& out) {
    out.resize(n);
    for (size_t i = 0; i < n; ++i) out[i] = std::complex<float>(input[i], 0.0f);

    size_t bits = 0;
    while ((size_t(1) << bits) < n) ++bits;

    for (size_t i = 0; i < n; ++i) {
        size_t j = 0;
        for (size_t k = 0; k < bits; ++k) {
            if (i & (size_t(1) << k)) j |= (size_t(1) << (bits - 1 - k));
        }
        if (j > i) std::swap(out[i], out[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * PI / static_cast<float>(len) * (inverse ? 1.0f : -1.0f);
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                std::complex<float> u = out[i + j];
                std::complex<float> v = out[i + j + len / 2] * w;
                out[i + j] = u + v;
                out[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        float inv_n = 1.0f / static_cast<float>(n);
        for (auto& c : out) c *= inv_n;
    }
}

void convolver_node::compute_fft_convolve(const std::vector<float>& signal,
                                           const std::vector<float>& kernel,
                                           std::vector<float>& result) {
    size_t conv_len = signal.size() + kernel.size() - 1;
    size_t fft_len = 1;
    while (fft_len < conv_len) fft_len *= 2;

    std::vector<std::complex<float>> signal_fft, kernel_fft;
    std::vector<float> padded_signal(fft_len, 0.0f);
    std::vector<float> padded_kernel(fft_len, 0.0f);
    memcpy(padded_signal.data(), signal.data(), signal.size() * sizeof(float));
    memcpy(padded_kernel.data(), kernel.data(), kernel.size() * sizeof(float));

    compute_fft(padded_signal.data(), fft_len, false, signal_fft);
    compute_fft(padded_kernel.data(), fft_len, false, kernel_fft);

    std::vector<std::complex<float>> product(fft_len);
    for (size_t i = 0; i < fft_len; ++i) product[i] = signal_fft[i] * kernel_fft[i];

    compute_fft(reinterpret_cast<float*>(product.data()), fft_len, true, signal_fft);

    result.resize(conv_len);
    for (size_t i = 0; i < conv_len; ++i) result[i] = signal_fft[i].real();
}

void convolver_node::process(const audio_buffer& input, audio_buffer& output, int frames) {
    output.fill(0.0f);
    if (!impulse_) return;

    int ch = std::min(input.channels, output.channels);
    ch = std::min(ch, impulse_->channels);

    for (int c = 0; c < ch; ++c) {
        std::vector<float> combined_input = prev_input_;
        combined_input.insert(combined_input.end(),
                              input.channel_data[c].begin(),
                              input.channel_data[c].begin() + frames);

        if (combined_input.size() >= static_cast<size_t>(fft_size_)) {
            std::vector<float> block(combined_input.end() - fft_size_, combined_input.end());
            std::vector<float> conv_result;
            compute_fft_convolve(block, impulse_->channel_data[c], conv_result);

            int write_frames = std::min(frames, static_cast<int>(conv_result.size()));
            for (int i = 0; i < write_frames; ++i) {
                output.channel_data[c][i] = conv_result[i];
            }

            prev_input_.clear();
        } else {
            prev_input_ = std::move(combined_input);
        }
    }
}

void convolver_node::reset() {
    prev_input_.clear();
}

// ---- GainNodeExt -----------------------------------------------------------

void gain_node_ext::process(const audio_buffer& input, audio_buffer& output, int frames) {
    int ch = std::min(input.channels, output.channels);
    for (int c = 0; c < ch; ++c) {
        for (int i = 0; i < frames; ++i) {
            output.channel_data[c][i] = input.channel_data[c][i] * gain_;
        }
    }
}

} // namespace hre::media
