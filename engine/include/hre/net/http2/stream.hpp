#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "hre/net/http2/frame.hpp"
#include "hre/net/http2/hpack.hpp"

namespace hre::net::http2 {

enum class stream_state {
    IDLE,
    RESERVED_LOCAL,
    RESERVED_REMOTE,
    OPEN,
    HALF_CLOSED_LOCAL,
    HALF_CLOSED_REMOTE,
    CLOSED
};

struct stream_priority {
    bool exclusive = false;
    uint32_t parent_stream_id = 0;
    uint8_t weight = 16;

    stream_priority() = default;
    stream_priority(bool ex, uint32_t parent, uint8_t w)
        : exclusive(ex), parent_stream_id(parent), weight(w) {}
};

class http2_stream {
public:
    http2_stream(uint32_t id, stream_priority priority = stream_priority());

    uint32_t id() const { return m_id; }
    stream_state state() const { return m_state; }
    void set_state(stream_state s) { m_state = s; }

    stream_priority& priority() { return m_priority; }
    const stream_priority& priority() const { return m_priority; }

    std::vector<header_entry>& request_headers() { return m_request_headers; }
    std::vector<header_entry>& response_headers() { return m_response_headers; }
    std::vector<uint8_t>& response_body() { return m_response_body; }

    int32_t window_size() const { return m_window_size; }
    void set_window_size(int32_t size) { m_window_size = size; }
    void decrease_window(int32_t amount) { m_window_size -= amount; }
    void increase_window(int32_t amount) { m_window_size += amount; }

    bool end_stream_received() const { return m_end_stream; }
    void set_end_stream(bool v) { m_end_stream = v; }

    void on_data(std::function<void(const std::vector<uint8_t>&)> cb) { m_data_cb = cb; }
    void on_headers(std::function<void(const std::vector<header_entry>&)> cb) { m_headers_cb = cb; }
    void on_close(std::function<void(error_code)> cb) { m_close_cb = cb; }

    void handle_data(const std::vector<uint8_t>& data);
    void handle_headers(const std::vector<header_entry>& headers);
    void handle_close(error_code ec);
    void handle_window_update(int32_t increment);

    void reset();

private:
    uint32_t m_id;
    stream_state m_state;
    stream_priority m_priority;
    std::vector<header_entry> m_request_headers;
    std::vector<header_entry> m_response_headers;
    std::vector<uint8_t> m_response_body;
    int32_t m_window_size;
    bool m_end_stream;

    std::function<void(const std::vector<uint8_t>&)> m_data_cb;
    std::function<void(const std::vector<header_entry>&)> m_headers_cb;
    std::function<void(error_code)> m_close_cb;
};

} // namespace hre::net::http2
