#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <winsock2.h>
#include "hre/net/http2/frame.hpp"
#include "hre/net/http2/hpack.hpp"
#include "hre/net/http2/stream.hpp"

namespace hre::net::http2 {

class http2_connection {
public:
    http2_connection();
    ~http2_connection();

    bool connect(const std::string& host, int port);
    void close();

    uint32_t open_stream(const std::vector<header_entry>& headers, bool end_stream = false);
    bool send_data(uint32_t stream_id, const std::vector<uint8_t>& data, bool end_stream = false);
    bool reset_stream(uint32_t stream_id, error_code ec = error_code::CANCEL);
    bool update_window(uint32_t stream_id, int32_t increment);

    std::shared_ptr<http2_stream> get_stream(uint32_t stream_id) const;
    bool is_connected() const { return m_connected; }
    uint32_t last_stream_id() const { return m_last_stream_id; }

    void on_frame(std::function<void(const frame&)> cb) { m_frame_cb = cb; }
    void on_error(std::function<void(error_code)> cb) { m_error_cb = cb; }

    void send_settings(const std::map<settings_id, uint32_t>& settings);
    void send_ping(const std::vector<uint8_t>& opaque_data);
    void send_goaway(uint32_t last_stream, error_code ec);

    void poll();

private:
    bool m_connected;
    SOCKET m_sock;
    std::string m_host;
    int m_port;

    uint32_t m_last_stream_id;
    uint32_t m_next_stream_id;
    int32_t m_connection_window;

    std::map<uint32_t, std::shared_ptr<http2_stream>> m_streams;
    std::queue<std::vector<uint8_t>> m_write_queue;

    hpack_encoder m_encoder;
    hpack_decoder m_decoder;

    std::vector<uint8_t> m_read_buffer;
    size_t m_read_offset;
    uint32_t m_expected_frame_length;
    bool m_reading_header;

    std::function<void(const frame&)> m_frame_cb;
    std::function<void(error_code)> m_error_cb;

    std::mutex m_mutex;

    bool perform_http2_upgrade();
    bool send_raw(const std::vector<uint8_t>& data);
    int read_raw(uint8_t* buffer, int len);
    void process_frame(const frame& f);
    void handle_settings(const frame& f);
    void handle_headers(const frame& f);
    void handle_data(const frame& f);
    void handle_rst_stream(const frame& f);
    void handle_goaway(const frame& f);
    void handle_window_update(const frame& f);
    void handle_ping(const frame& f);
    void handle_priority(const frame& f);
    void handle_push_promise(const frame& f);
    void flush_write_queue();
    void check_connection_window();
    void send_window_update(int32_t increment);
};

} // namespace hre::net::http2
