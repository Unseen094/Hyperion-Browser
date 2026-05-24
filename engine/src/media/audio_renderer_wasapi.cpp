#define _USE_MATH_DEFINES
#include <hre/media/audio_renderer_wasapi.hpp>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

namespace hre::media {

audio_renderer_wasapi::audio_renderer_wasapi() = default;
audio_renderer_wasapi::~audio_renderer_wasapi() { shutdown(); }

bool audio_renderer_wasapi::init(const audio_renderer_config& cfg) {
    if (m_initialized) return false;
    m_cfg = cfg;
    m_target_sample_rate = cfg.sample_rate;

    // Initialize sinc table
    if (!m_sinc_table_initialized) {
        for (int i = 0; i < SINC_TABLE_SIZE * SINC_HALF_LENGTH; ++i) {
            double phase = (double)i / SINC_TABLE_SIZE;
            if (phase == 0.0) {
                m_sinc_table[i] = 1.0f;
            } else {
                double x = M_PI * phase;
                double window = 0.5 * (1.0 + cos(x / SINC_HALF_LENGTH));
                m_sinc_table[i] = (float)(window * sin(x) / x);
            }
        }
        m_sinc_table_initialized = true;
    }

    // Initialize COM for audio
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                          CLSCTX_ALL, IID_PPV_ARGS(&m_enumerator));
    if (FAILED(hr)) return false;

    hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    if (FAILED(hr)) return false;

    hr = m_device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL,
                            nullptr, IID_PPV_ARGS_Helper(&m_audio_client));
    if (FAILED(hr)) return false;

    // Fill format
    m_wave_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    m_wave_format.Format.nChannels = cfg.channels;
    m_wave_format.Format.nSamplesPerSec = cfg.sample_rate;
    m_wave_format.Format.wBitsPerSample = cfg.bits_per_sample;
    m_wave_format.Format.nBlockAlign =
        m_wave_format.Format.nChannels * m_wave_format.Format.wBitsPerSample / 8;
    m_wave_format.Format.nAvgBytesPerSec =
        m_wave_format.Format.nSamplesPerSec * m_wave_format.Format.nBlockAlign;
    m_wave_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    if (cfg.channels == 2) {
        m_wave_format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    } else {
        m_wave_format.dwChannelMask = SPEAKER_FRONT_CENTER;
    }
    m_wave_format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    m_wave_format.Samples.wValidBitsPerSample = cfg.bits_per_sample;

    if (!init_audio_client()) return false;

    m_initialized = true;
    return true;
}

bool audio_renderer_wasapi::init_audio_client() {
    HRESULT hr;
    if (m_cfg.exclusive_mode) {
        hr = m_audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                         m_cfg.buffer_duration_ms * 10000,
                                         m_cfg.buffer_duration_ms * 10000,
                                         (WAVEFORMATEX*)&m_wave_format, nullptr);
    } else {
        hr = m_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                         m_cfg.buffer_duration_ms * 10000, 0,
                                         (WAVEFORMATEX*)&m_wave_format, nullptr);
    }
    if (FAILED(hr)) return false;

    hr = m_audio_client->GetService(IID_PPV_ARGS(&m_render_client));
    return SUCCEEDED(hr);
}

bool audio_renderer_wasapi::start() {
    if (!m_initialized || m_playing) return false;
    HRESULT hr = m_audio_client->Start();
    if (SUCCEEDED(hr)) {
        m_playing = true;
        // Start render thread (simplified: synchronous)
        render_audio();
    }
    return SUCCEEDED(hr);
}

bool audio_renderer_wasapi::stop() {
    if (!m_playing) return false;
    m_audio_client->Stop();
    m_audio_client->Reset();
    m_playing = false;
    return true;
}

bool audio_renderer_wasapi::pause() {
    if (!m_playing) return false;
    m_audio_client->Stop();
    m_playing = false;
    return true;
}

bool audio_renderer_wasapi::resume() {
    if (m_playing) return false;
    HRESULT hr = m_audio_client->Start();
    if (SUCCEEDED(hr)) m_playing = true;
    return SUCCEEDED(hr);
}

void audio_renderer_wasapi::set_volume(float vol) {
    m_volume = std::max(0.0f, std::min(1.0f, vol));
}

void audio_renderer_wasapi::set_pan(float pan) {
    m_pan = std::max(-1.0f, std::min(1.0f, pan));
}

void audio_renderer_wasapi::ramp_volume(float target_vol, int64_t duration_ms) {
    m_ramp_start_vol = m_volume;
    m_ramp_target_vol = target_vol;
    m_ramp_duration_ns = duration_ms * 1000000;
    m_ramp_start_time_ns = 0; // set to current time in real impl
    m_ramp_active = true;
}

float audio_renderer_wasapi::sinc_interpolate(float x) const {
    x = fabsf(x);
    if (x >= SINC_HALF_LENGTH) return 0.0f;
    int idx = (int)(x * SINC_TABLE_SIZE);
    if (idx >= SINC_TABLE_SIZE * SINC_HALF_LENGTH) return 0.0f;
    return m_sinc_table[idx];
}

void audio_renderer_wasapi::resample(const float* input, uint32_t input_frames,
                                      float* output, uint32_t& output_frames) {
    double ratio = (double)m_cfg.sample_rate / m_target_sample_rate;
    output_frames = (uint32_t)(input_frames * ratio) + 1;
    m_resample_buffer.resize(output_frames * m_cfg.channels);

    for (uint32_t i = 0; i < output_frames; ++i) {
        double src_pos = i / ratio;
        int idx = (int)src_pos;
        double frac = src_pos - idx;
        for (uint32_t ch = 0; ch < m_cfg.channels; ++ch) {
            if (idx + 1 < (int)input_frames) {
                float a = input[idx * m_cfg.channels + ch];
                float b = input[(idx + 1) * m_cfg.channels + ch];
                output[i * m_cfg.channels + ch] = a + (float)((b - a) * frac);
            }
        }
    }
}

bool audio_renderer_wasapi::submit_frame(const audio_frame& frame) {
    if (m_frame_queue.size() >= m_max_queue_frames) return false;
    m_frame_queue.push_back(frame);
    return true;
}

bool audio_renderer_wasapi::submit_frames(const std::vector<audio_frame>& frames) {
    for (auto& f : frames) {
        if (!submit_frame(f)) return false;
    }
    return true;
}

void audio_renderer_wasapi::fill_buffer(uint8_t* data, uint32_t frames) {
    uint32_t num_channels = m_cfg.channels;
    float* out = reinterpret_cast<float*>(data);
    uint32_t total_samples = frames * num_channels;

    if (m_mix_buffer.size() < total_samples) {
        m_mix_buffer.resize(total_samples, 0.0f);
    }

    // Mix from queued frames
    uint32_t samples_written = 0;
    while (samples_written < total_samples && !m_frame_queue.empty()) {
        auto& frame = m_frame_queue.front();
        uint32_t frame_samples = (uint32_t)frame.samples.size();
        uint32_t to_copy = std::min(frame_samples, total_samples - samples_written);
        memcpy(m_mix_buffer.data() + samples_written, frame.samples.data(),
               to_copy * sizeof(float));
        samples_written += to_copy;

        if (to_copy >= frame_samples) {
            m_frame_queue.erase(m_frame_queue.begin());
        } else {
            frame.samples.erase(frame.samples.begin(),
                                 frame.samples.begin() + to_copy);
        }
    }

    if (samples_written == 0) {
        memset(data, 0, total_samples * sizeof(float));
        return;
    }

    // Apply volume, pan, volume ramp
    float current_vol = m_volume;
    if (m_ramp_active) {
        int64_t elapsed = 0; // would compute from system clock
        double t = (double)elapsed / m_ramp_duration_ns;
        if (t >= 1.0) {
            current_vol = m_ramp_target_vol;
            m_volume = m_ramp_target_vol;
            m_ramp_active = false;
        } else {
            current_vol = m_ramp_start_vol + (float)((m_ramp_target_vol - m_ramp_start_vol) * t);
        }
    }

    float left_gain = current_vol * std::min(1.0f, 1.0f - m_pan);
    float right_gain = current_vol * std::min(1.0f, 1.0f + m_pan);

    for (uint32_t i = 0; i < samples_written; i += num_channels) {
        float left = m_mix_buffer[i];
        float right = (num_channels > 1) ? m_mix_buffer[i + 1] : left;
        left *= left_gain;
        right *= right_gain;
        out[i] = left;
        if (num_channels > 1) out[i + 1] = right;
    }

    // Fill remaining with silence
    if (samples_written < total_samples) {
        memset(out + samples_written, 0,
               (total_samples - samples_written) * sizeof(float));
    }
}

bool audio_renderer_wasapi::render_audio() {
    if (!m_render_client) return false;

    UINT32 buffer_frames = 0;
    HRESULT hr = m_audio_client->GetBufferSize(&buffer_frames);
    if (FAILED(hr)) return false;

    UINT32 padding = 0;
    hr = m_audio_client->GetCurrentPadding(&padding);
    if (FAILED(hr)) return false;

    UINT32 available = buffer_frames - padding;
    if (available == 0) return true;

    BYTE* buffer = nullptr;
    hr = m_render_client->GetBuffer(available, &buffer);
    if (FAILED(hr) || !buffer) return false;

    fill_buffer(buffer, available);

    hr = m_render_client->ReleaseBuffer(available, 0);
    return SUCCEEDED(hr);
}

bool audio_renderer_wasapi::set_target_sample_rate(uint32_t sample_rate) {
    m_target_sample_rate = sample_rate;
    return true;
}

uint32_t audio_renderer_wasapi::buffer_fill_ms() const {
    (void)m_audio_client;
    return 0;
}

bool audio_renderer_wasapi::is_buffer_low() const {
    return m_frame_queue.size() < 4;
}

bool audio_renderer_wasapi::is_buffer_full() const {
    return m_frame_queue.size() >= m_max_queue_frames;
}

void audio_renderer_wasapi::shutdown() {
    if (m_playing) stop();
    m_render_client.Reset();
    m_audio_client.Reset();
    m_device.Reset();
    m_enumerator.Reset();
    m_initialized = false;
}

} // namespace hre::media
