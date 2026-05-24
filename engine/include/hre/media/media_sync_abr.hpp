#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <memory>
#include <functional>
#include <string>

namespace hre::media {

struct av_sync_config {
    double min_playback_rate = 0.5;
    double max_playback_rate = 2.0;
    double av_diff_threshold_ms = 40.0;
    double max_av_diff_ms = 100.0;
    bool adjust_playback_rate = true;
};

struct abr_config {
    double min_buffer_seconds = 5.0;
    double max_buffer_seconds = 60.0;
    double low_buffer_threshold = 10.0;
    double high_buffer_threshold = 45.0;
    double startup_buffer_seconds = 3.0;
    double rebuffer_penalty_seconds = 5.0;
    int min_bitrate_kbps = 100;
    int max_bitrate_kbps = 20000;
    int default_bitrate_kbps = 2000;
};

struct media_stream_info {
    int bitrate_kbps = 0;
    int width = 0;
    int height = 0;
    double framerate = 0.0;
    std::string codec;
    std::string url;
};

struct decoded_sample {
    int64_t pts_ns = 0;
    int64_t dts_ns = 0;
    int64_t duration_ns = 0;
    bool is_keyframe = false;
    int track_id = 0;
    std::vector<uint8_t> data;
};

class media_sync_abr {
public:
    media_sync_abr();
    ~media_sync_abr();

    // AV Sync
    void set_sync_config(const av_sync_config& cfg);
    av_sync_config sync_config() const { return m_av_cfg; }

    bool push_video_sample(const decoded_sample& sample);
    bool push_audio_sample(const decoded_sample& sample);

    decoded_sample next_video_sample();
    decoded_sample next_audio_sample();

    double current_playback_rate() const { return m_playback_rate; }
    int64_t current_pts() const { return m_current_pts_ns; }
    int64_t current_dts() const { return m_current_dts_ns; }

    bool is_audio_ahead() const;
    bool is_video_ahead() const;
    void resync();

    // Adaptive Bitrate (BBA algorithm)
    void set_abr_config(const abr_config& cfg);
    abr_config abr_config() const { return m_abr_cfg; }
    void set_available_bitrates(const std::vector<media_stream_info>& streams);
    int select_bitrate_bba(double current_buffer_level);
    void report_buffer_update(double buffer_level_seconds);
    double buffer_level() const { return m_buffer_level_seconds; }
    int current_bitrate_kbps() const { return m_current_bitrate_kbps; }

    // Seek-to-keyframe with preroll
    bool seek(int64_t target_ns);
    int64_t seek_target() const { return m_seek_target_ns; }
    bool is_seeking() const { return m_seeking; }
    int64_t next_keyframe_pts() const;
    double preroll_progress() const;

    // Throughput estimation
    void report_download_speed(size_t bytes, int64_t duration_ns);
    double estimated_throughput_kbps() const { return m_estimated_throughput; }

    // Event callbacks
    using rate_change_callback = std::function<void(double new_rate)>;
    using bitrate_change_callback = std::function<void(int new_bitrate_kbps)>;
    void on_playback_rate_change(rate_change_callback cb) { m_rate_callback = std::move(cb); }
    void on_bitrate_change(bitrate_change_callback cb) { m_bitrate_callback = std::move(cb); }

private:
    double compute_bba_bitrate(double buffer_level) const;
    int64_t find_nearest_keyframe(const std::deque<decoded_sample>& samples,
                                   int64_t target_ns) const;

    av_sync_config m_av_cfg;
    abr_config m_abr_cfg;

    std::deque<decoded_sample> m_video_queue;
    std::deque<decoded_sample> m_audio_queue;
    static const size_t MAX_QUEUE_SIZE = 128;

    int64_t m_current_pts_ns = 0;
    int64_t m_current_dts_ns = 0;
    double m_playback_rate = 1.0;
    int64_t m_av_diff_ns = 0;

    // ABR state
    double m_buffer_level_seconds = 0.0;
    int m_current_bitrate_kbps = 2000;
    double m_estimated_throughput = 5000.0;
    std::vector<media_stream_info> m_available_streams;

    // Seek state
    bool m_seeking = false;
    int64_t m_seek_target_ns = 0;
    int64_t m_seek_keyframe_ns = 0;
    double m_preroll_progress = 0.0;

    // Callbacks
    rate_change_callback m_rate_callback;
    bitrate_change_callback m_bitrate_callback;
};

} // namespace hre::media
