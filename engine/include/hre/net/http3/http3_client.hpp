#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include "hre/net/http3/quic_connection.hpp"
#include "hre/net/http3/quic_congestion.hpp"

namespace hre::net::http3 {

struct http3_response {
    int status_code = 0;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    bool success = false;
    std::string error_message;
    uint64_t connection_rtt = 0;
};

struct qpack_instruction {
    enum type_t { INDEXED, LITERAL_NAME_REF, LITERAL, DUP_ENTRY, DYNAMIC_TABLE_SIZE };
    type_t type;
    uint32_t index;
    std::string name;
    std::string value;
};

class http3_client {
public:
    http3_client();
    ~http3_client();

    void fetch(const std::string& url, std::function<void(const http3_response&)> callback);
    void set_congestion_control(const std::string& algo);

    bool is_supported(const std::string& host, int port);
    void close_all();

    static bool is_quic_supported();

private:
    struct quic_session {
        std::shared_ptr<quic_connection> conn;
        std::shared_ptr<congestion_controller> cc;
        std::string host;
        int port;
    };

    std::map<std::string, std::shared_ptr<quic_session>> m_sessions;

    std::string parse_host(const std::string& url);
    int parse_port(const std::string& url);
    std::string parse_path(const std::string& url);
    bool is_https(const std::string& url);

    std::shared_ptr<quic_session> get_or_create_session(const std::string& host, int port);
    std::vector<uint8_t> encode_qpack_headers(const std::vector<std::pair<std::string, std::string>>& headers);
    std::vector<std::pair<std::string, std::string>> decode_qpack_headers(const std::vector<uint8_t>& data);
};

} // namespace hre::net::http3
