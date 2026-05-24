#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace hre::media {

enum class source_buffer_append_mode {
    SEGMENTS,
    SEQUENCE
};

struct media_source_config {
    bool live_duration = false;
    double duration = 0.0;
};

struct source_buffer_config {
    std::string mime_type;
    source_buffer_append_mode mode = source_buffer_append_mode::SEGMENTS;
    double timestamp_offset = 0.0;
    double append_window_start = 0.0;
    double append_window_end = INFINITY;
    bool generate_timestamps = false;
};

struct media_segment {
    std::vector<uint8_t> data;
    double pts = 0.0;
    double dts = 0.0;
    double duration = 0.0;
    bool is_keyframe = false;
    int64_t track_id = 0;
};

class media_source_ext {
public:
    media_source_ext();
    ~media_source_ext();

    // MSE lifecycle
    bool open(const media_source_config& cfg);
    void close();
    bool is_open() const { return m_open; }

    double duration() const { return m_duration; }
    void set_duration(double d);

    // Source buffer management
    uint32_t add_source_buffer(const source_buffer_config& cfg);
    void remove_source_buffer(uint32_t id);
    void append_buffer(uint32_t buffer_id, const uint8_t* data, size_t len);
    void abort_buffer(uint32_t buffer_id);
    void remove_range(uint32_t buffer_id, double start, double end);

    // Timestamp offset
    void set_timestamp_offset(uint32_t buffer_id, double offset);
    double timestamp_offset(uint32_t buffer_id) const;

    // Append window
    void set_append_window(uint32_t buffer_id, double start, double end);
    double append_window_start(uint32_t buffer_id) const;
    double append_window_end(uint32_t buffer_id) const;

    // Adaptive bitrate selection (BBA algorithm)
    void set_bitrate_adaptation_enabled(bool en) { m_abr_enabled = en; }
    int select_bitrate(const std::vector<int>& bitrates,
                       double current_buffer_level,
                       double max_buffer_size);

    // Buffer level tracking
    double buffer_level(uint32_t buffer_id) const;
    void set_buffer_size_limit(double seconds) { m_max_buffer_seconds = seconds; }

    // Event callbacks
    using update_callback = std::function<void(uint32_t buffer_id)>;
    using error_callback = std::function<void(uint32_t buffer_id, const std::string&)>;
    void on_update(uint32_t buffer_id, update_callback cb);
    void on_error(uint32_t buffer_id, error_callback cb);

    // Segment access for downstream consumption
    size_t pending_segment_count(uint32_t buffer_id) const;
    media_segment next_segment(uint32_t buffer_id);

private:
    struct source_buffer_impl {
        source_buffer_config config;
        std::vector<uint8_t> raw_buffer;
        std::vector<media_segment> segments;
        double current_pts = 0.0;
        bool updating = false;
        update_callback on_update_cb;
        error_callback on_error_cb;
    };

    bool m_open = false;
    double m_duration = 0.0;
    bool m_abr_enabled = false;
    double m_max_buffer_seconds = 60.0;
    uint32_t m_next_buffer_id = 1;
    std::vector<std::unique_ptr<source_buffer_impl>> m_buffers;
};

} // namespace hre::media
