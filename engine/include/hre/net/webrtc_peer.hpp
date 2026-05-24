#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <map>
#include <array>
#include <mutex>
#include <winsock2.h>

namespace hre::net {

struct ice_candidate {
    std::string candidate;
    std::string sdp_mid;
    int sdp_mline_index = 0;
};

struct session_description {
    std::string type;
    std::string sdp;
};

enum class ice_state {
    NEW,
    CHECKING,
    CONNECTED,
    COMPLETED,
    FAILED,
    DISCONNECTED,
    CLOSED
};

enum class dtls_state {
    NEW,
    CONNECTING,
    CONNECTED,
    CLOSED,
    FAILED
};

enum class data_channel_state {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

struct data_channel {
    int id;
    std::string label;
    data_channel_state state = data_channel_state::CONNECTING;
    std::vector<uint8_t> pending_data;
};

struct stun_message {
    enum class method : uint16_t {
        BINDING = 0x0001
    };
    enum class class_type : uint16_t {
        REQUEST = 0x0000,
        INDICATION = 0x0010,
        SUCCESS_RESPONSE = 0x0100,
        ERROR_RESPONSE = 0x0110
    };

    class_type msg_class = class_type::REQUEST;
    method msg_method = method::BINDING;
    std::array<uint8_t, 12> transaction_id{};
    std::map<uint16_t, std::vector<uint8_t>> attributes;
};

class webrtc_peer {
public:
    using ice_cb = std::function<void(const ice_candidate&)>;
    using track_cb = std::function<void(const std::string&)>;
    using message_cb = std::function<void(const uint8_t*, size_t)>;
    using state_cb = std::function<void(ice_state)>;

    webrtc_peer();
    ~webrtc_peer();

    void set_on_ice_candidate(ice_cb cb) { on_ice_ = std::move(cb); }
    void set_on_track(track_cb cb) { on_track_ = std::move(cb); }
    void set_on_message(message_cb cb) { on_message_ = std::move(cb); }
    void set_on_connection_state(state_cb cb) { on_state_ = std::move(cb); }

    session_description create_offer();
    session_description create_answer();
    void set_remote_description(const session_description& sdp);
    void add_ice_candidate(const ice_candidate& cand);
    void send_data(const uint8_t* data, size_t len);

    bool ice_start(const std::string& stun_server, uint16_t port);
    void ice_restart();
    void ice_stop();

    dtls_state dtls() const { return dtls_state_; }
    ice_state connection_state() const { return ice_state_; }

    int create_data_channel(const std::string& label);
    bool send_data_channel(int channel_id, const uint8_t* data, size_t len);
    void close_data_channel(int channel_id);

private:
    stun_message create_stun_binding();
    std::vector<uint8_t> serialize_stun(const stun_message& msg);
    bool parse_stun(const uint8_t* data, size_t len, stun_message& msg);
    bool send_stun(const stun_message& msg);
    bool recv_stun(stun_message& msg);

    bool perform_dtls_handshake();
    bool dtls_read(uint8_t* buf, size_t len, size_t& read);
    bool dtls_write(const uint8_t* buf, size_t len);

    bool parse_sdp(const std::string& sdp, std::map<std::string, std::string>& fields);
    std::string generate_sdp(const std::string& type);

    void dispatch_ice_candidate(const ice_candidate& cand);
    void transition_state(ice_state new_state);
    bool udp_send(const uint8_t* data, size_t len, const sockaddr_in& addr);

    std::string local_sdp_;
    std::string remote_sdp_;

    ice_cb on_ice_;
    track_cb on_track_;
    message_cb on_message_;
    state_cb on_state_;

    ice_state ice_state_ = ice_state::NEW;
    dtls_state dtls_state_ = dtls_state::NEW;

    bool ice_complete_ = false;
    bool dtls_complete_ = false;

    std::string stun_server_;
    uint16_t stun_port_ = 3478;
    SOCKET stun_sock_ = INVALID_SOCKET;
    sockaddr_in stun_addr_{};

    std::mutex mtx_;
    std::vector<uint8_t> dtls_buf_;
    std::map<int, data_channel> data_channels_;
    int next_channel_id_ = 0;
};

} // namespace hre::net
