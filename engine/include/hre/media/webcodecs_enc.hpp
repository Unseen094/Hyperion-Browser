#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <map>

namespace hre::media {

enum class encoder_state {
    UNCONFIGURED,
    CONFIGURED,
    ENCODING,
    CLOSED
};

struct video_encoder_config {
    int width = 640;
    int height = 480;
    int bitrate = 1000000;
    int framerate = 30;
    std::string codec = "vp8";
};

struct encoded_video_chunk {
    std::vector<uint8_t> data;
    int64_t timestamp_us = 0;
    int64_t duration_us = 0;
    bool key_frame = false;
};

struct audio_encoder_config {
    int sample_rate = 48000;
    int channels = 2;
    int bitrate = 64000;
    std::string codec = "opus";
};

struct encoded_audio_chunk {
    std::vector<uint8_t> data;
    int64_t timestamp_us = 0;
    bool key_frame = false;
};

class video_frame {
public:
    video_frame() = default;
    video_frame(int w, int h) : width_(w), height_(h) {
        y_plane_.resize(w * h);
        u_plane_.resize(w * h / 4);
        v_plane_.resize(w * h / 4);
    }

    int width() const { return width_; }
    int height() const { return height_; }

    uint8_t* y_data() { return y_plane_.data(); }
    uint8_t* u_data() { return u_plane_.data(); }
    uint8_t* v_data() { return v_plane_.data(); }

    const uint8_t* y_data() const { return y_plane_.data(); }
    const uint8_t* u_data() const { return u_plane_.data(); }
    const uint8_t* v_data() const { return v_plane_.data(); }

    size_t y_size() const { return y_plane_.size(); }
    size_t uv_size() const { return u_plane_.size(); }

    void fill_yuv(uint8_t y, uint8_t u, uint8_t v) {
        std::fill(y_plane_.begin(), y_plane_.end(), y);
        std::fill(u_plane_.begin(), u_plane_.end(), u);
        std::fill(v_plane_.begin(), v_plane_.end(), v);
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> y_plane_;
    std::vector<uint8_t> u_plane_;
    std::vector<uint8_t> v_plane_;
};

class video_encoder {
public:
    using output_cb = std::function<void(const encoded_video_chunk&)>;
    using error_cb = std::function<void(const std::string&)>;

    video_encoder();
    ~video_encoder();

    encoder_state state() const { return state_; }

    void configure(const video_encoder_config& config);
    void encode(const video_frame& frame);
    void flush();
    void close();

    void set_output_callback(output_cb cb) { on_output_ = std::move(cb); }
    void set_error_callback(error_cb cb) { on_error_ = std::move(cb); }

private:
    void encode_vp8(const video_frame& frame);
    void encode_h264(const video_frame& frame);

    encoder_state state_ = encoder_state::UNCONFIGURED;
    video_encoder_config config_;
    output_cb on_output_;
    error_cb on_error_;
    std::vector<uint8_t> bitstream_;
};

class audio_data {
public:
    audio_data() = default;
    audio_data(int sr, int ch, int frames)
        : sample_rate_(sr), channels_(ch), frames_(frames) {
        data_.resize(ch * frames, 0.0f);
    }

    int sample_rate() const { return sample_rate_; }
    int channels() const { return channels_; }
    int frames() const { return frames_; }
    float* channel_data(int ch) { return data_.data() + ch * frames_; }
    const float* channel_data(int ch) const { return data_.data() + ch * frames_; }
    size_t total_samples() const { return data_.size(); }

private:
    int sample_rate_ = 48000;
    int channels_ = 2;
    int frames_ = 0;
    std::vector<float> data_;
};

class audio_encoder {
public:
    using output_cb = std::function<void(const encoded_audio_chunk&)>;
    using error_cb = std::function<void(const std::string&)>;

    audio_encoder();
    ~audio_encoder();

    encoder_state state() const { return state_; }

    void configure(const audio_encoder_config& config);
    void encode(const audio_data& data);
    void flush();
    void close();

    void set_output_callback(output_cb cb) { on_output_ = std::move(cb); }
    void set_error_callback(error_cb cb) { on_error_ = std::move(cb); }

private:
    void encode_opus(const audio_data& data);

    encoder_state state_ = encoder_state::UNCONFIGURED;
    audio_encoder_config config_;
    output_cb on_output_;
    error_cb on_error_;
    std::vector<uint8_t> packet_;
};

} // namespace hre::media
