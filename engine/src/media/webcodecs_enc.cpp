#include <hre/media/webcodecs_enc.hpp>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace hre::media {

static void compress_dct_block(const float block[64], int16_t out[64]) {
    for (int i = 0; i < 64; ++i) {
        out[i] = static_cast<int16_t>(std::clamp(block[i] * 8.0f, -32768.0f, 32767.0f));
    }
}

static void yuv420_to_i420(const video_frame& src, std::vector<uint8_t>& i420) {
    size_t y_size = src.y_size();
    size_t uv_size = src.uv_size();
    i420.resize(y_size + uv_size * 2);
    memcpy(i420.data(), src.y_data(), y_size);
    memcpy(i420.data() + y_size, src.u_data(), uv_size);
    memcpy(i420.data() + y_size + uv_size, src.v_data(), uv_size);
}

video_encoder::video_encoder() = default;
video_encoder::~video_encoder() { close(); }

void video_encoder::configure(const video_encoder_config& config) {
    config_ = config;
    state_ = encoder_state::CONFIGURED;
}

void video_encoder::encode(const video_frame& frame) {
    if (state_ != encoder_state::CONFIGURED && state_ != encoder_state::ENCODING) return;
    if (state_ == encoder_state::CONFIGURED) state_ = encoder_state::ENCODING;

    if (config_.codec == "vp8" || config_.codec == "vp9") {
        encode_vp8(frame);
    } else {
        encode_h264(frame);
    }
}

void video_encoder::flush() {
    if (state_ != encoder_state::ENCODING) return;
    if (on_output_ && !bitstream_.empty()) {
        encoded_video_chunk chunk;
        chunk.data = std::move(bitstream_);
        chunk.key_frame = true;
        on_output_(chunk);
        bitstream_.clear();
    }
}

void video_encoder::close() {
    flush();
    state_ = encoder_state::CLOSED;
}

void video_encoder::encode_vp8(const video_frame& frame) {
    std::vector<uint8_t> i420;
    yuv420_to_i420(frame, i420);

    std::vector<uint8_t> packet;
    packet.push_back(0x9D);
    packet.push_back(0x01);
    packet.push_back(0x2A);

    int q = std::clamp(config_.bitrate / 100000, 0, 127);
    packet.push_back(static_cast<uint8_t>(q));

    for (size_t i = 0; i < i420.size(); i += 16) {
        int16_t block[64];
        float dct[64];
        for (int j = 0; j < 64; ++j) {
            dct[j] = (i + j < i420.size()) ? (static_cast<float>(i420[i + j]) - 128.0f) / 256.0f : 0.0f;
        }
        compress_dct_block(dct, block);
        for (int j = 0; j < 64; j += 2) {
            packet.push_back(static_cast<uint8_t>(block[j] & 0xFF));
            packet.push_back(static_cast<uint8_t>((block[j] >> 8) & 0xFF));
        }
    }

    encoded_video_chunk chunk;
    chunk.data = std::move(packet);
    chunk.key_frame = true;
    chunk.timestamp_us = 0;
    chunk.duration_us = 1000000 / config_.framerate;

    if (on_output_) on_output_(chunk);
}

void video_encoder::encode_h264(const video_frame& frame) {
    std::vector<uint8_t> i420;
    yuv420_to_i420(frame, i420);

    std::vector<uint8_t> packet;
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x01);
    packet.insert(packet.end(), i420.begin(), i420.end());

    encoded_video_chunk chunk;
    chunk.data = std::move(packet);
    chunk.key_frame = true;
    if (on_output_) on_output_(chunk);
}

audio_encoder::audio_encoder() = default;
audio_encoder::~audio_encoder() { close(); }

void audio_encoder::configure(const audio_encoder_config& config) {
    config_ = config;
    state_ = encoder_state::CONFIGURED;
}

void audio_encoder::encode(const audio_data& data) {
    if (state_ != encoder_state::CONFIGURED && state_ != encoder_state::ENCODING) return;
    if (state_ == encoder_state::CONFIGURED) state_ = encoder_state::ENCODING;
    encode_opus(data);
}

void audio_encoder::flush() {
    if (state_ != encoder_state::ENCODING) return;
    if (on_output_ && !packet_.empty()) {
        encoded_audio_chunk chunk;
        chunk.data = std::move(packet_);
        on_output_(chunk);
        packet_.clear();
    }
}

void audio_encoder::close() {
    flush();
    state_ = encoder_state::CLOSED;
}

void audio_encoder::encode_opus(const audio_data& data) {
    packet_.clear();
    int frame_size = 960;
    int total_frames = data.frames();

    packet_.push_back(0xFC);
    packet_.push_back(static_cast<uint8_t>(config_.channels));
    packet_.push_back(static_cast<uint8_t>(config_.sample_rate / 1000));

    for (int i = 0; i < total_frames; i += frame_size) {
        int n = std::min(frame_size, total_frames - i);
        for (int c = 0; c < config_.channels; ++c) {
            const float* ch_data = data.channel_data(c) + i;
            for (int s = 0; s < n; ++s) {
                int16_t sample = static_cast<int16_t>(std::clamp(ch_data[s] * 32767.0f, -32768.0f, 32767.0f));
                packet_.push_back(static_cast<uint8_t>(sample & 0xFF));
                packet_.push_back(static_cast<uint8_t>((sample >> 8) & 0xFF));
            }
        }
    }

    if (on_output_) {
        encoded_audio_chunk chunk;
        chunk.data = packet_;
        chunk.timestamp_us = 0;
        on_output_(chunk);
    }
}

} // namespace hre::media
