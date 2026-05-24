#include "hre/net/http2/connection.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace hre::net::http2 {

http2_connection::http2_connection()
    : m_connected(false)
    , m_sock(INVALID_SOCKET)
    , m_port(443)
    , m_last_stream_id(0)
    , m_next_stream_id(1)
    , m_connection_window(65535)
    , m_read_offset(0)
    , m_expected_frame_length(0)
    , m_reading_header(true)
{}

http2_connection::~http2_connection() {
    close();
}

bool http2_connection::connect(const std::string& host, int port) {
    m_host = host;
    m_port = port;

    addrinfo hints = {}, *info;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &info) != 0)
        return false;

    m_sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (m_sock == INVALID_SOCKET) {
        freeaddrinfo(info);
        return false;
    }

    if (::connect(m_sock, info->ai_addr, (int)info->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(info);
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    freeaddrinfo(info);

    if (!perform_http2_upgrade()) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    m_connected = true;
    return true;
}

void http2_connection::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected = false;
    for (auto& [id, stream] : m_streams) {
        stream->handle_close(error_code::H2_NO_ERROR);
    }
    m_streams.clear();
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
}

uint32_t http2_connection::open_stream(const std::vector<header_entry>& headers, bool end_stream) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected) return 0;

    uint32_t stream_id = m_next_stream_id;
    m_next_stream_id += 2;
    m_last_stream_id = stream_id;

    auto stream = std::make_shared<http2_stream>(stream_id);
    stream->set_state(stream_state::OPEN);
    stream->set_end_stream(end_stream);
    m_streams[stream_id] = stream;

    auto encoded = m_encoder.encode(headers);

    frame f;
    f.header.type = frame_type::HEADERS;
    f.header.flags = static_cast<uint8_t>(frame_flag::END_HEADERS);
    if (end_stream) f.header.flags |= static_cast<uint8_t>(frame_flag::END_STREAM);
    f.header.stream_id = stream_id;
    f.payload = std::move(encoded);
    f.header.length = static_cast<uint32_t>(f.payload.size());

    m_write_queue.push(f.serialize());
    flush_write_queue();

    return stream_id;
}

bool http2_connection::send_data(uint32_t stream_id, const std::vector<uint8_t>& data, bool end_stream) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected) return false;

    auto it = m_streams.find(stream_id);
    if (it == m_streams.end()) return false;

    const uint32_t max_frame_size = 16384;
    size_t offset = 0;
    while (offset < data.size()) {
        size_t chunk = std::min<size_t>(data.size() - offset, max_frame_size);
        frame f;
        f.header.type = frame_type::DATA;
        f.header.stream_id = stream_id;
        if (offset + chunk >= data.size() && end_stream)
            f.header.flags |= static_cast<uint8_t>(frame_flag::END_STREAM);
        f.payload.assign(data.begin() + offset, data.begin() + offset + chunk);
        f.header.length = static_cast<uint32_t>(f.payload.size());
        m_write_queue.push(f.serialize());
        offset += chunk;
    }
    flush_write_queue();
    return true;
}

bool http2_connection::reset_stream(uint32_t stream_id, error_code ec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end()) return false;

    std::vector<uint8_t> payload;
    uint32_t code = static_cast<uint32_t>(ec);
    payload.push_back(static_cast<uint8_t>((code >> 24) & 0xFF));
    payload.push_back(static_cast<uint8_t>((code >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(code & 0xFF));

    frame f;
    f.header.type = frame_type::RST_STREAM;
    f.header.stream_id = stream_id;
    f.payload = std::move(payload);
    f.header.length = static_cast<uint32_t>(f.payload.size());
    m_write_queue.push(f.serialize());

    it->second->handle_close(ec);
    m_streams.erase(it);
    flush_write_queue();
    return true;
}

bool http2_connection::update_window(uint32_t stream_id, int32_t increment) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (increment <= 0) return false;

    frame f;
    f.header.type = frame_type::WINDOW_UPDATE;
    f.header.stream_id = stream_id;

    std::vector<uint8_t> payload;
    uint32_t field = static_cast<uint32_t>(increment) & 0x7FFFFFFF;
    payload.push_back(static_cast<uint8_t>((field >> 24) & 0xFF));
    payload.push_back(static_cast<uint8_t>((field >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((field >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(field & 0xFF));
    f.payload = std::move(payload);
    f.header.length = static_cast<uint32_t>(f.payload.size());

    m_write_queue.push(f.serialize());
    flush_write_queue();
    return true;
}

std::shared_ptr<http2_stream> http2_connection::get_stream(uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    return (it != m_streams.end()) ? it->second : nullptr;
}

void http2_connection::send_settings(const std::map<settings_id, uint32_t>& settings) {
    settings_frame sf;
    sf.settings = settings;
    auto payload = sf.serialize();

    frame f;
    f.header.type = frame_type::SETTINGS;
    f.payload = std::move(payload);
    f.header.length = static_cast<uint32_t>(f.payload.size());

    std::lock_guard<std::mutex> lock(m_mutex);
    m_write_queue.push(f.serialize());
    flush_write_queue();
}

void http2_connection::send_ping(const std::vector<uint8_t>& opaque_data) {
    frame f;
    f.header.type = frame_type::PING;
    f.payload = opaque_data;
    f.payload.resize(8);
    f.header.length = 8;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_write_queue.push(f.serialize());
    flush_write_queue();
}

void http2_connection::send_goaway(uint32_t last_stream, error_code ec) {
    goaway_frame gf;
    gf.last_stream_id = last_stream;
    gf.code = ec;
    auto payload = gf.serialize();

    frame f;
    f.header.type = frame_type::GOAWAY;
    f.payload = std::move(payload);
    f.header.length = static_cast<uint32_t>(f.payload.size());

    std::lock_guard<std::mutex> lock(m_mutex);
    m_write_queue.push(f.serialize());
    flush_write_queue();
}

void http2_connection::poll() {
    if (!m_connected) return;

    uint8_t header_buf[9];
    int n = read_raw(header_buf, 9);
    if (n <= 0) {
        if (n == 0) {
            close();
        }
        return;
    }

    std::vector<uint8_t> buf(header_buf, header_buf + 9);
    size_t off = 0;
    frame f = frame::deserialize(buf, off);

    if (f.header.length > 0) {
        f.payload.resize(f.header.length);
        int total_read = 0;
        while (total_read < static_cast<int>(f.header.length)) {
            n = read_raw(f.payload.data() + total_read,
                         static_cast<int>(f.header.length) - total_read);
            if (n <= 0) break;
            total_read += n;
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    process_frame(f);
}

bool http2_connection::perform_http2_upgrade() {
    // Send HTTP/1.1 request with Upgrade: h2c for cleartext
    std::string upgrade = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    send_raw(std::vector<uint8_t>(upgrade.begin(), upgrade.end()));

    // Read preface
    uint8_t preface[24];
    int n = read_raw(preface, 24);
    if (n < 24) return false;

    std::string expected_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    if (std::string(reinterpret_cast<char*>(preface), 24) != expected_preface)
        return false;

    // Send initial SETTINGS (empty = defaults)
    frame settings_frame;
    settings_frame.header.type = frame_type::SETTINGS;
    settings_frame.header.length = 0;
    auto sf_data = settings_frame.serialize();
    send_raw(sf_data);

    return true;
}

bool http2_connection::send_raw(const std::vector<uint8_t>& data) {
    if (m_sock == INVALID_SOCKET) return false;
    int sent = ::send(m_sock, reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()), 0);
    return sent == static_cast<int>(data.size());
}

int http2_connection::read_raw(uint8_t* buffer, int len) {
    if (m_sock == INVALID_SOCKET) return -1;
    return ::recv(m_sock, reinterpret_cast<char*>(buffer), len, 0);
}

void http2_connection::process_frame(const frame& f) {
    if (m_frame_cb) m_frame_cb(f);

    switch (f.header.type) {
        case frame_type::SETTINGS:
            handle_settings(f);
            break;
        case frame_type::HEADERS:
            handle_headers(f);
            break;
        case frame_type::DATA:
            handle_data(f);
            break;
        case frame_type::RST_STREAM:
            handle_rst_stream(f);
            break;
        case frame_type::GOAWAY:
            handle_goaway(f);
            break;
        case frame_type::WINDOW_UPDATE:
            handle_window_update(f);
            break;
        case frame_type::PING:
            handle_ping(f);
            break;
        case frame_type::PRIORITY:
            handle_priority(f);
            break;
        case frame_type::PUSH_PROMISE:
            handle_push_promise(f);
            break;
        default:
            break;
    }
}

void http2_connection::handle_settings(const frame& f) {
    if (f.header.stream_id != 0) return;
    auto sf = settings_frame::deserialize(f.payload);
    for (const auto& [id, val] : sf.settings) {
        if (id == settings_id::INITIAL_WINDOW_SIZE) {
            m_connection_window = static_cast<int32_t>(val);
        } else if (id == settings_id::MAX_FRAME_SIZE) {
            m_expected_frame_length = val;
        }
    }
    // Send SETTINGS acknowledgment
    frame ack;
    ack.header.type = frame_type::SETTINGS;
    ack.header.flags = 0x01; // ACK
    ack.header.length = 0;
    m_write_queue.push(ack.serialize());
    flush_write_queue();
}

void http2_connection::handle_headers(const frame& f) {
    auto stream = get_stream(f.header.stream_id);
    if (!stream) {
        stream = std::make_shared<http2_stream>(f.header.stream_id);
        m_streams[f.header.stream_id] = stream;
    }

    size_t offset = 0;
    auto headers = m_decoder.decode(f.payload, offset);
    stream->handle_headers(headers);

    if (f.header.flags & static_cast<uint8_t>(frame_flag::END_STREAM)) {
        stream->set_end_stream(true);
    }
}

void http2_connection::handle_data(const frame& f) {
    auto stream = get_stream(f.header.stream_id);
    if (!stream) return;

    m_connection_window -= static_cast<int32_t>(f.payload.size());
    stream->decrease_window(static_cast<int32_t>(f.payload.size()));
    stream->handle_data(f.payload);

    // Auto flow control: send WINDOW_UPDATE for consumed data
    send_window_update(static_cast<int32_t>(f.payload.size()));

    if (f.header.flags & static_cast<uint8_t>(frame_flag::END_STREAM)) {
        stream->set_end_stream(true);
        stream->set_state(stream_state::HALF_CLOSED_REMOTE);
    }
}

void http2_connection::handle_rst_stream(const frame& f) {
    auto stream = get_stream(f.header.stream_id);
    if (!stream) return;

    if (f.payload.size() >= 4) {
        uint32_t code = (static_cast<uint32_t>(f.payload[0]) << 24)
                      | (static_cast<uint32_t>(f.payload[1]) << 16)
                      | (static_cast<uint32_t>(f.payload[2]) << 8)
                      | static_cast<uint32_t>(f.payload[3]);
        stream->handle_close(static_cast<error_code>(code));
    }
    m_streams.erase(f.header.stream_id);
}

void http2_connection::handle_goaway(const frame& f) {
    auto gf = goaway_frame::deserialize(f.payload);
    m_last_stream_id = gf.last_stream_id;

    // Close all streams with ID > last_stream_id
    auto it = m_streams.begin();
    while (it != m_streams.end()) {
        if (it->first > m_last_stream_id) {
            it->second->handle_close(gf.code);
            it = m_streams.erase(it);
        } else {
            ++it;
        }
    }

    if (m_error_cb) m_error_cb(gf.code);
}

void http2_connection::handle_window_update(const frame& f) {
    auto wf = window_update_frame::deserialize(f.payload);
    if (f.header.stream_id == 0) {
        m_connection_window += static_cast<int32_t>(wf.window_size_increment);
    } else {
        auto stream = get_stream(f.header.stream_id);
        if (stream) {
            stream->handle_window_update(static_cast<int32_t>(wf.window_size_increment));
        }
    }
}

void http2_connection::handle_ping(const frame& f) {
    if (f.header.flags & 0x01) return; // ACK, ignore
    // Send PING ACK
    frame ack;
    ack.header.type = frame_type::PING;
    ack.header.flags = 0x01;
    ack.payload = f.payload;
    ack.header.length = 8;
    m_write_queue.push(ack.serialize());
    flush_write_queue();
}

void http2_connection::handle_priority(const frame& f) {
    auto pf = priority_frame::deserialize(f.payload);
    auto stream = get_stream(f.header.stream_id);
    if (stream) {
        stream->priority().exclusive = pf.exclusive;
        stream->priority().parent_stream_id = pf.stream_id;
        stream->priority().weight = pf.weight;
    }
}

void http2_connection::handle_push_promise(const frame& f) {
    size_t offset = 0;
    uint32_t promised_stream_id = (static_cast<uint32_t>(f.payload[offset]) << 24)
                                | (static_cast<uint32_t>(f.payload[offset + 1]) << 16)
                                | (static_cast<uint32_t>(f.payload[offset + 2]) << 8)
                                | static_cast<uint32_t>(f.payload[offset + 3]);
    offset += 4;
    promised_stream_id &= 0x7FFFFFFF;

    auto promised = std::make_shared<http2_stream>(promised_stream_id);
    promised->set_state(stream_state::RESERVED_REMOTE);
    m_streams[promised_stream_id] = promised;
}

void http2_connection::flush_write_queue() {
    while (!m_write_queue.empty()) {
        if (!send_raw(m_write_queue.front())) break;
        m_write_queue.pop();
    }
}

void http2_connection::check_connection_window() {
    if (m_connection_window < 65535) {
        send_window_update(65535 - m_connection_window);
    }
}

void http2_connection::send_window_update(int32_t increment) {
    if (increment <= 0) return;
    uint32_t field = static_cast<uint32_t>(increment) & 0x7FFFFFFF;
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>((field >> 24) & 0xFF));
    payload.push_back(static_cast<uint8_t>((field >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((field >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(field & 0xFF));

    frame f;
    f.header.type = frame_type::WINDOW_UPDATE;
    f.header.stream_id = 0;
    f.payload = std::move(payload);
    f.header.length = 4;
    m_write_queue.push(f.serialize());
}

} // namespace hre::net::http2
