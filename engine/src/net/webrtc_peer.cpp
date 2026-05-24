#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <hre/net/webrtc_peer.hpp>
#include <algorithm>
#include <sstream>
#include <random>
#include <array>
#include <cstring>
#include <chrono>
#include <thread>

namespace hre::net {

static std::vector<uint8_t> generate_transaction_id() {
    std::vector<uint8_t> id(12);
    std::random_device rd;
    for (auto& b : id) b = static_cast<uint8_t>(rd());
    return id;
}

static uint16_t stun_htons(uint16_t v) {
    return htons(v);
}

static uint32_t stun_htonl(uint32_t v) {
    return htonl(v);
}

webrtc_peer::webrtc_peer() {
    stun_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

webrtc_peer::~webrtc_peer() {
    ice_stop();
}

session_description webrtc_peer::create_offer() {
    std::lock_guard<std::mutex> lock(mtx_);
    local_sdp_ = generate_sdp("offer");
    return {"offer", local_sdp_};
}

session_description webrtc_peer::create_answer() {
    std::lock_guard<std::mutex> lock(mtx_);
    local_sdp_ = generate_sdp("answer");
    return {"answer", local_sdp_};
}

void webrtc_peer::set_remote_description(const session_description& sdp) {
    std::lock_guard<std::mutex> lock(mtx_);
    remote_sdp_ = sdp.sdp;
}

void webrtc_peer::add_ice_candidate(const ice_candidate& cand) {
    std::lock_guard<std::mutex> lock(mtx_);
    dispatch_ice_candidate(cand);
}

void webrtc_peer::send_data(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (dtls_state_ != dtls_state::CONNECTED) return;
    dtls_write(data, len);
}

bool webrtc_peer::ice_start(const std::string& stun_server, uint16_t port) {
    std::lock_guard<std::mutex> lock(mtx_);
    stun_server_ = stun_server;
    stun_port_ = port;

    if (stun_sock_ == INVALID_SOCKET) {
        stun_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (stun_sock_ == INVALID_SOCKET) return false;
    }

    stun_addr_.sin_family = AF_INET;
    stun_addr_.sin_port = htons(port);
    stun_addr_.sin_addr.s_addr = inet_addr(stun_server.c_str());
    if (stun_addr_.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* he = gethostbyname(stun_server.c_str());
        if (!he) return false;
        memcpy(&stun_addr_.sin_addr, he->h_addr_list[0], he->h_length);
    }

    transition_state(ice_state::CHECKING);

    stun_message binding = create_stun_binding();
    if (!send_stun(binding)) {
        transition_state(ice_state::FAILED);
        return false;
    }

    stun_message response;
    if (recv_stun(response)) {
        transition_state(ice_state::CONNECTED);
        ice_complete_ = true;

        if (!perform_dtls_handshake()) {
            transition_state(ice_state::FAILED);
            return false;
        }
    } else {
        transition_state(ice_state::FAILED);
        return false;
    }

    return true;
}

void webrtc_peer::ice_restart() {
    std::lock_guard<std::mutex> lock(mtx_);
    ice_complete_ = false;
    dtls_complete_ = false;
    dtls_state_ = dtls_state::NEW;
    transition_state(ice_state::NEW);
    local_sdp_.clear();
    remote_sdp_.clear();

    if (stun_sock_ != INVALID_SOCKET) {
        closesocket(stun_sock_);
        stun_sock_ = INVALID_SOCKET;
    }

    ice_start(stun_server_, stun_port_);
}

void webrtc_peer::ice_stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (stun_sock_ != INVALID_SOCKET) {
        closesocket(stun_sock_);
        stun_sock_ = INVALID_SOCKET;
    }
    ice_complete_ = false;
    dtls_complete_ = false;
    ice_state_ = ice_state::CLOSED;
    dtls_state_ = dtls_state::CLOSED;
}

int webrtc_peer::create_data_channel(const std::string& label) {
    std::lock_guard<std::mutex> lock(mtx_);
    int id = next_channel_id_++;
    data_channel dc;
    dc.id = id;
    dc.label = label;
    dc.state = data_channel_state::OPEN;
    data_channels_[id] = dc;
    return id;
}

bool webrtc_peer::send_data_channel(int channel_id, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = data_channels_.find(channel_id);
    if (it == data_channels_.end() || it->second.state != data_channel_state::OPEN) return false;

    std::vector<uint8_t> frame;
    frame.push_back(static_cast<uint8_t>(channel_id & 0xFF));
    frame.insert(frame.end(), data, data + len);
    dtls_write(frame.data(), frame.size());
    return true;
}

void webrtc_peer::close_data_channel(int channel_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = data_channels_.find(channel_id);
    if (it != data_channels_.end()) {
        it->second.state = data_channel_state::CLOSED;
    }
}

stun_message webrtc_peer::create_stun_binding() {
    stun_message msg;
    msg.msg_class = stun_message::class_type::REQUEST;
    msg.msg_method = stun_message::method::BINDING;
    auto id_bytes = generate_transaction_id();
    memcpy(msg.transaction_id.data(), id_bytes.data(), 12);
    return msg;
}

std::vector<uint8_t> webrtc_peer::serialize_stun(const stun_message& msg) {
    std::vector<uint8_t> buf;

    uint16_t type = static_cast<uint16_t>(msg.msg_method) | static_cast<uint16_t>(msg.msg_class);
    type = stun_htons(type);
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&type), reinterpret_cast<uint8_t*>(&type) + 2);

    uint16_t len = 0;
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 2);

    buf.insert(buf.end(), msg.transaction_id.begin(), msg.transaction_id.end());

    uint16_t attr_type = stun_htons(0x0000);
    for (const auto& [attr, val] : msg.attributes) {
        attr_type = stun_htons(attr);
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&attr_type), reinterpret_cast<uint8_t*>(&attr_type) + 2);
        uint16_t attr_len = stun_htons(static_cast<uint16_t>(val.size()));
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&attr_len), reinterpret_cast<uint8_t*>(&attr_len) + 2);
        buf.insert(buf.end(), val.begin(), val.end());
        while (buf.size() % 4 != 0) buf.push_back(0);
    }

    uint16_t total_len = static_cast<uint16_t>(buf.size() - 20);
    buf[2] = static_cast<uint8_t>((total_len >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(total_len & 0xFF);

    return buf;
}

bool webrtc_peer::parse_stun(const uint8_t* data, size_t len, stun_message& msg) {
    if (len < 20) return false;

    uint16_t type = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    uint16_t msg_len = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    if (len < static_cast<size_t>(msg_len) + 20) return false;

    msg.msg_method = static_cast<stun_message::method>(type & 0x001F);
    msg.msg_class = static_cast<stun_message::class_type>(type & 0x0110);
    memcpy(msg.transaction_id.data(), data + 4, 12);

    size_t offset = 20;
    while (offset + 4 <= len) {
        uint16_t attr_type = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        uint16_t attr_len = (static_cast<uint16_t>(data[offset + 2]) << 8) | data[offset + 3];
        offset += 4;
        if (offset + attr_len > len) break;
        msg.attributes[attr_type] = std::vector<uint8_t>(data + offset, data + offset + attr_len);
        offset += attr_len;
        while (offset % 4 != 0) ++offset;
    }

    return true;
}

bool webrtc_peer::send_stun(const stun_message& msg) {
    if (stun_sock_ == INVALID_SOCKET) return false;
    auto wire = serialize_stun(msg);
    return udp_send(wire.data(), wire.size(), stun_addr_);
}

bool webrtc_peer::recv_stun(stun_message& msg) {
    if (stun_sock_ == INVALID_SOCKET) return false;

    uint8_t buf[1024];
    sockaddr_in from{};
    int fromlen = sizeof(from);

    timeval tv{};
    tv.tv_sec = 3;
    setsockopt(stun_sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

    int n = recvfrom(stun_sock_, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                     reinterpret_cast<sockaddr*>(&from), &fromlen);
    if (n <= 0) return false;

    return parse_stun(buf, static_cast<size_t>(n), msg);
}

bool webrtc_peer::perform_dtls_handshake() {
    dtls_state_ = dtls_state::CONNECTING;

    std::vector<uint8_t> client_hello;
    client_hello.push_back(22);
    client_hello.push_back(254);
    client_hello.push_back(253);
    client_hello.insert(client_hello.end(), {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    client_hello.insert(client_hello.end(), 32, 0x00);

    if (!dtls_write(client_hello.data(), client_hello.size())) {
        dtls_state_ = dtls_state::FAILED;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    dtls_state_ = dtls_state::CONNECTED;
    dtls_complete_ = true;
    return true;
}

bool webrtc_peer::dtls_read(uint8_t* buf, size_t len, size_t& read) {
    if (stun_sock_ == INVALID_SOCKET) return false;

    timeval tv{};
    tv.tv_sec = 1;
    setsockopt(stun_sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

    sockaddr_in from{};
    int fromlen = sizeof(from);
    int n = recvfrom(stun_sock_, reinterpret_cast<char*>(buf), static_cast<int>(len), 0,
                     reinterpret_cast<sockaddr*>(&from), &fromlen);
    if (n <= 0) return false;
    read = static_cast<size_t>(n);
    return true;
}

bool webrtc_peer::dtls_write(const uint8_t* buf, size_t len) {
    if (stun_sock_ == INVALID_SOCKET) return false;
    return udp_send(buf, len, stun_addr_);
}

bool webrtc_peer::parse_sdp(const std::string& sdp, std::map<std::string, std::string>& fields) {
    std::istringstream stream(sdp);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        char key = line[0];
        std::string value = (line.size() > 2) ? line.substr(2) : "";
        fields[std::string(1, key)] = value;
    }
    return true;
}

std::string webrtc_peer::generate_sdp(const std::string& type) {
    std::ostringstream sdp;
    sdp << "v=0\r\n";
    sdp << "o=- 0 0 IN IP4 0.0.0.0\r\n";
    sdp << "s=HyperionWebRTC\r\n";
    sdp << "t=0 0\r\n";
    sdp << "a=group:BUNDLE 0\r\n";
    sdp << "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n";
    sdp << "c=IN IP4 0.0.0.0\r\n";
    sdp << "a=mid:0\r\n";
    sdp << "a=sendrecv\r\n";
    sdp << "a=sctp-port:5000\r\n";
    sdp << "a=max-message-size:262144\r\n";
    sdp << "a=" << type << "\r\n";
    return sdp.str();
}

void webrtc_peer::dispatch_ice_candidate(const ice_candidate& cand) {
    if (on_ice_) on_ice_(cand);
}

void webrtc_peer::transition_state(ice_state new_state) {
    ice_state_ = new_state;
    if (new_state == ice_state::CONNECTED) ice_complete_ = true;
    if (on_state_) on_state_(new_state);
}

bool webrtc_peer::udp_send(const uint8_t* data, size_t len, const sockaddr_in& addr) {
    int n = sendto(stun_sock_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0,
                   reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    return n > 0;
}

} // namespace hre::net
