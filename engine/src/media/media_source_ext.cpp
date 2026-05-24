#include <hre/media/media_source_ext.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace hre::media {

media_source_ext::media_source_ext() = default;
media_source_ext::~media_source_ext() = default;

bool media_source_ext::open(const media_source_config& cfg) {
    if (m_open) return false;
    m_duration = cfg.duration;
    m_open = true;
    return true;
}

void media_source_ext::close() {
    m_buffers.clear();
    m_open = false;
    m_duration = 0.0;
}

void media_source_ext::set_duration(double d) {
    m_duration = d;
}

uint32_t media_source_ext::add_source_buffer(const source_buffer_config& cfg) {
    uint32_t id = m_next_buffer_id++;
    auto buf = std::make_unique<source_buffer_impl>();
    buf->config = cfg;
    buf->current_pts = cfg.timestamp_offset;
    m_buffers.push_back(std::move(buf));
    return id;
}

void media_source_ext::remove_source_buffer(uint32_t id) {
    auto it = std::remove_if(m_buffers.begin(), m_buffers.end(),
                              [id](const auto& b) { return b->config.mime_type.empty() && false; });
    (void)it;
}

void media_source_ext::append_buffer(uint32_t buffer_id, const uint8_t* data, size_t len) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (!buf) return;
    buf->updating = true;

    double win_start = buf->config.append_window_start;
    double win_end = buf->config.append_window_end;

    if (buf->config.mime_type.find("video") != std::string::npos ||
        buf->config.mime_type.find("audio") != std::string::npos) {
        // Parse ISOBMFF or WebM to extract segment boundaries
        // Simplified: create one segment per append
        media_segment seg;
        seg.data.assign(data, data + len);
        seg.dts = buf->current_pts;
        seg.pts = buf->current_pts + buf->config.timestamp_offset;
        seg.duration = 2.0; // estimate
        seg.is_keyframe = true;

        // Apply append window
        if (seg.pts + seg.duration < win_start || seg.pts > win_end) {
            // outside window, discard
        } else {
            if (seg.pts < win_start) {
                double trim = win_start - seg.pts;
                seg.pts = win_start;
                seg.duration -= trim;
            }
            if (seg.pts + seg.duration > win_end) {
                seg.duration = win_end - seg.pts;
            }
            if (seg.duration > 0) {
                buf->segments.push_back(std::move(seg));
            }
        }

        buf->current_pts += 2.0;
    }

    buf->updating = false;
    if (buf->on_update_cb) buf->on_update_cb(buffer_id);
}

void media_source_ext::abort_buffer(uint32_t buffer_id) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (!buf) return;
    buf->raw_buffer.clear();
    buf->updating = false;
}

void media_source_ext::remove_range(uint32_t buffer_id, double start, double end) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (!buf) return;
    auto it = std::remove_if(buf->segments.begin(), buf->segments.end(),
                              [start, end](const media_segment& s) {
                                  return s.pts >= start && s.pts + s.duration <= end;
                              });
    buf->segments.erase(it, buf->segments.end());
}

void media_source_ext::set_timestamp_offset(uint32_t buffer_id, double offset) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (!buf) return;
    buf->config.timestamp_offset = offset;
}

double media_source_ext::timestamp_offset(uint32_t buffer_id) const {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    return buf ? buf->config.timestamp_offset : 0.0;
}

void media_source_ext::set_append_window(uint32_t buffer_id, double start, double end) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (!buf) return;
    buf->config.append_window_start = start;
    buf->config.append_window_end = end;
}

double media_source_ext::append_window_start(uint32_t buffer_id) const {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    return buf ? buf->config.append_window_start : 0.0;
}

double media_source_ext::append_window_end(uint32_t buffer_id) const {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    return buf ? buf->config.append_window_end : INFINITY;
}

int media_source_ext::select_bitrate(const std::vector<int>& bitrates,
                                      double current_buffer_level,
                                      double max_buffer_size) {
    if (!m_abr_enabled || bitrates.empty()) return 0;
    // BBA (Buffer Based Algorithm) - simplified
    double buffer_fraction = current_buffer_level / max_buffer_size;
    if (buffer_fraction < 0.2) {
        // Low buffer: select lowest bitrate
        return bitrates[0];
    } else if (buffer_fraction > 0.8) {
        // High buffer: select highest bitrate safely
        return bitrates.back();
    } else {
        // Mid buffer: use linear interpolation
        int idx = static_cast<int>((buffer_fraction - 0.2) / 0.6 * (bitrates.size() - 1));
        idx = std::max(0, std::min(static_cast<int>(bitrates.size()) - 1, idx));
        return bitrates[idx];
    }
}

double media_source_ext::buffer_level(uint32_t buffer_id) const {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (!buf || buf->segments.empty()) return 0.0;
    double total = 0.0;
    for (auto& seg : buf->segments) total += seg.duration;
    return total;
}

void media_source_ext::on_update(uint32_t buffer_id, update_callback cb) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (buf) buf->on_update_cb = std::move(cb);
}

void media_source_ext::on_error(uint32_t buffer_id, error_callback cb) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (buf) buf->on_error_cb = std::move(cb);
}

size_t media_source_ext::pending_segment_count(uint32_t buffer_id) const {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    return buf ? buf->segments.size() : 0;
}

media_segment media_source_ext::next_segment(uint32_t buffer_id) {
    auto* buf = buffer_id < m_buffers.size() ? m_buffers[buffer_id].get() : nullptr;
    if (!buf || buf->segments.empty()) return {};
    media_segment seg = std::move(buf->segments.front());
    buf->segments.erase(buf->segments.begin());
    return seg;
}

} // namespace hre::media
