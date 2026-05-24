#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <functional>
#include <wrl/client.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

namespace hre::media {

struct audio_renderer_config {
    uint32_t sample_rate = 48000;
    uint16_t channels = 2;
    uint16_t bits_per_sample = 32;
    bool exclusive_mode = true;
    bool use_float_output = true;
    uint32_t buffer_duration_ms = 50;
};

struct audio_frame {
    std::vector<float> samples; // interleaved float32
    int64_t pts_ns = 0;
    int64_t duration_ns = 0;
    bool is_continuous = true;
};

class audio_renderer_wasapi {
public:
    audio_renderer_wasapi();
    ~audio_renderer_wasapi();

    bool init(const audio_renderer_config& cfg);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Playback control
    bool start();
    bool stop();
    bool pause();
    bool resume();
    bool is_playing() const { return m_playing; }

    // Volume and pan
    void set_volume(float vol); // 0.0 - 1.0
    float volume() const { return m_volume; }
    void set_pan(float pan);   // -1.0 left, 0 center, 1.0 right
    float pan() const { return m_pan; }

    // Volume ramp
    void ramp_volume(float target_vol, int64_t duration_ms);

    // Submit audio data
    bool submit_frame(const audio_frame& frame);
    bool submit_frames(const std::vector<audio_frame>& frames);

    // Sinc resampler
    bool set_target_sample_rate(uint32_t sample_rate);
    uint32_t target_sample_rate() const { return m_target_sample_rate; }

    // Buffer state
    uint32_t buffer_fill_ms() const;
    bool is_buffer_low() const;
    bool is_buffer_full() const;

    // Event callbacks
    using buffer_callback = std::function<void()>;
    void on_buffer_needed(buffer_callback cb) { m_buffer_callback = std::move(cb); }

private:
    bool init_audio_client();
    bool render_audio();
    void fill_buffer(uint8_t* data, uint32_t frames);
    float sinc_interpolate(float x) const;
    void resample(const float* input, uint32_t input_frames,
                  float* output, uint32_t& output_frames);

    // COM interfaces
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;
    Microsoft::WRL::ComPtr<IMMDevice> m_device;
    Microsoft::WRL::ComPtr<IAudioClient3> m_audio_client;
    Microsoft::WRL::ComPtr<IAudioRenderClient> m_render_client;

    // Configuration
    audio_renderer_config m_cfg;
    WAVEFORMATEXTENSIBLE m_wave_format;

    // State
    bool m_initialized = false;
    bool m_playing = false;
    float m_volume = 1.0f;
    float m_pan = 0.0f;

    // Sample rate conversion
    uint32_t m_target_sample_rate = 48000;
    std::vector<float> m_resample_buffer;

    // Volume ramp
    float m_ramp_start_vol = 1.0f;
    float m_ramp_target_vol = 1.0f;
    int64_t m_ramp_start_time_ns = 0;
    int64_t m_ramp_duration_ns = 0;
    bool m_ramp_active = false;

    // Frame queue
    std::vector<audio_frame> m_frame_queue;
    uint32_t m_max_queue_frames = 16;

    // Internal buffer
    std::vector<float> m_mix_buffer;

    // Callback
    buffer_callback m_buffer_callback;

    // Sinc table
    static const int SINC_TABLE_SIZE = 1024;
    static const int SINC_HALF_LENGTH = 16;
    float m_sinc_table[SINC_TABLE_SIZE * SINC_HALF_LENGTH];
    bool m_sinc_table_initialized = false;
};

} // namespace hre::media
