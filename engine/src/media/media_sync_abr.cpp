#include <hre/media/media_sync_abr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace hre::media {

media_sync_abr::media_sync_abr() {
    m_av_cfg.min_playback_rate = 0.5;
    m_av_cfg.max_playback_rate = 2.0;
    m_av_cfg.av_diff_threshold_ms = 40.0;
    m_av_cfg.max_av_diff_ms = 100.0;
    m_av_cfg.adjust_playback_rate = true;

    m_abr_cfg.min_buffer_seconds = 5.0;
    m_abr_cfg.max_buffer_seconds = 60.0;
    m_abr_cfg.low_buffer_threshold = 10.0;
    m_abr_cfg.high_buffer_threshold = 45.0;
    m_abr_cfg.startup_buffer_seconds = 3.0;
    m_abr_cfg.rebuffer_penalty_seconds = 5.0;
    m_abr_cfg.min_bitrate_kbps = 100;
    m_abr_cfg.max_bitrate_kbps = 20000;
    m_abr_cfg.default_bitrate_kbps = 2000;
}

media_sync_abr::~media_sync_abr() = default;

void media_sync_abr::set_sync_config(const av_sync_config& cfg) { m_av_cfg = cfg; }

// ---- AV Sync ---------------------------------------------------------------

bool media_sync_abr::push_video_sample(const decoded_sample& sample) {
    if (m_video_queue.size() >= MAX_QUEUE_SIZE) return false;
    m_video_queue.push_back(sample);
    return true;
}

bool media_sync_abr::push_audio_sample(const decoded_sample& sample) {
    if (m_audio_queue.size() >= MAX_QUEUE_SIZE) return false;
    m_audio_queue.push_back(sample);
    return true;
}

decoded_sample media_sync_abr::next_video_sample() {
    if (m_video_queue.empty()) return {};
    decoded_sample s = std::move(m_video_queue.front());
    m_video_queue.pop_front();
    m_current_pts_ns = s.pts_ns;
    m_current_dts_ns = s.dts_ns;

    // Check AV sync
    if (!m_audio_queue.empty()) {
        int64_t audio_pts = m_audio_queue.front().pts_ns;
        m_av_diff_ns = s.pts_ns - audio_pts;

        if (m_av_cfg.adjust_playback_rate &&
            std::abs(m_av_diff_ns) > m_av_cfg.av_diff_threshold_ms * 1000000) {
            double drift = (double)m_av_diff_ns / 1000000.0;
            double rate_adjust = 1.0 + drift * 0.001;
            m_playback_rate = std::max(m_av_cfg.min_playback_rate,
                              std::min(m_av_cfg.max_playback_rate, rate_adjust));
        }
    }
    return s;
}

decoded_sample media_sync_abr::next_audio_sample() {
    if (m_audio_queue.empty()) return {};
    decoded_sample s = std::move(m_audio_queue.front());
    m_audio_queue.pop_front();
    return s;
}

bool media_sync_abr::is_audio_ahead() const {
    return m_av_diff_ns < -m_av_cfg.max_av_diff_ms * 1000000;
}

bool media_sync_abr::is_video_ahead() const {
    return m_av_diff_ns > m_av_cfg.max_av_diff_ms * 1000000;
}

void media_sync_abr::resync() {
    if (m_audio_queue.empty() || m_video_queue.empty()) return;
    int64_t audio_pts = m_audio_queue.front().pts_ns;
    int64_t video_pts = m_video_queue.front().pts_ns;
    int64_t diff = video_pts - audio_pts;

    if (diff > m_av_cfg.max_av_diff_ms * 1000000) {
        // Video too far ahead: drop video frames
        while (!m_video_queue.empty() && m_video_queue.front().pts_ns > audio_pts + 50000000) {
            m_video_queue.pop_front();
        }
    } else if (diff < -m_av_cfg.max_av_diff_ms * 1000000) {
        // Audio too far ahead: skip audio
        while (!m_audio_queue.empty() && m_audio_queue.front().pts_ns < video_pts - 50000000) {
            m_audio_queue.pop_front();
        }
    }
    m_playback_rate = 1.0;
}

// ---- Adaptive Bitrate (BBA) ------------------------------------------------

void media_sync_abr::set_abr_config(const abr_config& cfg) { m_abr_cfg = cfg; }

void media_sync_abr::set_available_bitrates(const std::vector<media_stream_info>& streams) {
    m_available_streams = streams;
}

double media_sync_abr::compute_bba_bitrate(double buffer_level) const {
    // BBA-1 algorithm: linear interpolation based on buffer occupancy
    double b_low = m_abr_cfg.low_buffer_threshold;
    double b_high = m_abr_cfg.high_buffer_threshold;
    double r_min = (double)m_abr_cfg.min_bitrate_kbps;
    double r_max = (double)m_abr_cfg.max_bitrate_kbps;

    if (buffer_level <= b_low) return r_min;
    if (buffer_level >= b_high) return r_max;

    double fraction = (buffer_level - b_low) / (b_high - b_low);
    return r_min + fraction * (r_max - r_min);
}

int media_sync_abr::select_bitrate_bba(double current_buffer_level) {
    m_buffer_level_seconds = current_buffer_level;

    // Apply rebuffer penalty
    if (current_buffer_level < m_abr_cfg.startup_buffer_seconds) {
        current_buffer_level -= m_abr_cfg.rebuffer_penalty_seconds;
        if (current_buffer_level < 0) current_buffer_level = 0;
    }

    double target_bitrate = compute_bba_bitrate(current_buffer_level);
    target_bitrate = std::min(target_bitrate, m_estimated_throughput * 0.8);

    // Snap to nearest available bitrate
    int selected = m_abr_cfg.default_bitrate_kbps;
    if (m_available_streams.empty()) {
        selected = (int)target_bitrate;
    } else {
        int min_diff = INT_MAX;
        for (auto& stream : m_available_streams) {
            int diff = std::abs(stream.bitrate_kbps - (int)target_bitrate);
            if (diff < min_diff && stream.bitrate_kbps <= target_bitrate) {
                min_diff = diff;
                selected = stream.bitrate_kbps;
            }
        }
        if (min_diff == INT_MAX && !m_available_streams.empty()) {
            selected = m_available_streams[0].bitrate_kbps;
        }
    }

    if (selected != m_current_bitrate_kbps) {
        m_current_bitrate_kbps = selected;
        if (m_bitrate_callback) m_bitrate_callback(selected);
    }

    return selected;
}

void media_sync_abr::report_buffer_update(double buffer_level_seconds) {
    m_buffer_level_seconds = buffer_level_seconds;
    select_bitrate_bba(buffer_level_seconds);
}

// ---- Seek-to-Keyframe with Preroll ----------------------------------------

bool media_sync_abr::seek(int64_t target_ns) {
    m_seeking = true;
    m_seek_target_ns = target_ns;
    m_preroll_progress = 0.0;
    m_playback_rate = 1.0;

    // Clear queues
    m_video_queue.clear();
    m_audio_queue.clear();

    // Find nearest keyframe
    m_seek_keyframe_ns = target_ns;
    // In a real implementation, scan the index for the nearest keyframe
    m_preroll_progress = 0.1;
    return true;
}

int64_t media_sync_abr::next_keyframe_pts() const {
    return m_seek_keyframe_ns;
}

double media_sync_abr::preroll_progress() const {
    return m_preroll_progress;
}

void media_sync_abr::report_download_speed(size_t bytes, int64_t duration_ns) {
    if (duration_ns <= 0) return;
    double speed_kbps = (double)bytes * 8.0 / (duration_ns / 1000000.0);
    // Exponentially weighted moving average
    const double alpha = 0.3;
    m_estimated_throughput = (1.0 - alpha) * m_estimated_throughput + alpha * speed_kbps;
}

} // namespace hre::media
