#include "hre/net/http2/stream.hpp"
#include <algorithm>

namespace hre::net::http2 {

http2_stream::http2_stream(uint32_t id, stream_priority priority)
    : m_id(id)
    , m_state(stream_state::IDLE)
    , m_priority(priority)
    , m_window_size(65535)
    , m_end_stream(false)
{}

void http2_stream::handle_data(const std::vector<uint8_t>& data) {
    m_response_body.insert(m_response_body.end(), data.begin(), data.end());
    if (m_data_cb) m_data_cb(data);
}

void http2_stream::handle_headers(const std::vector<header_entry>& headers) {
    m_response_headers = headers;
    for (const auto& h : headers) {
        if (h.name == ":status" && !h.value.empty()) {
            m_state = stream_state::OPEN;
        }
    }
    if (m_headers_cb) m_headers_cb(headers);
}

void http2_stream::handle_close(error_code ec) {
    m_state = stream_state::CLOSED;
    if (m_close_cb) m_close_cb(ec);
}

void http2_stream::handle_window_update(int32_t increment) {
    m_window_size += increment;
}

void http2_stream::reset() {
    m_state = stream_state::CLOSED;
    m_response_headers.clear();
    m_response_body.clear();
    m_request_headers.clear();
}

} // namespace hre::net::http2
