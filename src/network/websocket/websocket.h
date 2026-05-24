#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <cstdint>
#include <functional>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace hyperion::network::websocket {

// WebSocket close codes
enum class WebSocketCloseCode : uint16_t {
    NORMAL_CLOSURE = 1000,
    GOING_AWAY = 1001,
    PROTOCOL_ERROR = 1002,
    UNSUPPORTED_DATA = 1003,
    NO_STATUS_RECEIVED = 1005,
    ABNORMAL_CLOSURE = 1006,
    INVALID_FRAME_PAYLOAD = 1007,
    POLICY_VIOLATION = 1008,
    MESSAGE_TOO_BIG = 1009,
    MISSING_EXTENSION = 1010,
    INTERNAL_ERROR = 1011,
    SERVICE_RESTART = 1012,
    TRY_AGAIN_LATER = 1013,
};

// WebSocket frame opcodes
enum class WebSocketOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA,
};

// WebSocket frame header
struct WebSocketFrameHeader {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;
    WebSocketOpcode opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
};

// WebSocket frame
class WebSocketFrame {
public:
    static std::string create_frame(const std::string& message, WebSocketOpcode opcode = WebSocketOpcode::TEXT) {
        WebSocketFrameHeader header{};
        header.fin = true;
        header.opcode = opcode;
        header.mask = false; // Client receives unmasked
        
        // For small payloads use 7-bit length
        if (message.size() <= 125) {
            header.payload_length = message.size();
        } else {
            header.payload_length = 126;
        }
        
        // Build frame
        std::string frame;
        frame += static_cast<char>(
            (header.fin ? 0x80 : 0) |
            (header.rsv1 ? 0x40 : 0) |
            (header.rsv2 ? 0x20 : 0) |
            (header.rsv3 ? 0x10 : 0) |
            static_cast<uint8_t>(header.opcode)
        );
        
        // Payload length and masking
        if (header.payload_length <= 125) {
            frame += static_cast<char>(header.payload_length);
        } else {
            frame += static_cast<char>(126);
            frame += static_cast<char>((header.payload_length >> 8) & 0xFF);
            frame += static_cast<char>(header.payload_length & 0xFF);
        }
        
        // No masking for server->client frames
        frame += message;
        
        return frame;
    }
    
    static std::pair<WebSocketOpcode, std::string> parse_frame(const std::string& frame) {
        if (frame.empty() || frame.size() < 2) {
            throw std::runtime_error("Invalid WebSocket frame: too short");
        }
        
        WebSocketFrameHeader header{};
        header.fin = (frame[0] & 0x80) != 0;
        header.rsv1 = (frame[0] & 0x40) != 0;
        header.rsv2 = (frame[0] & 0x20) != 0;
        header.rsv3 = (frame[0] & 0x10) != 0;
        header.opcode = static_cast<WebSocketOpcode>(frame[0] & 0x0F);
        header.mask = (frame[1] & 0x80) != 0;
        
        size_t payload_offset = 2;
        uint64_t payload_length = frame[1] & 0x7F;
        
        if (payload_length == 126) {
            if (frame.size() < 4) {
                throw std::runtime_error("Invalid WebSocket frame: incomplete 16-bit length");
            }
            payload_length = (static_cast<uint8_t>(frame[2]) << 8) | 
                           static_cast<uint8_t>(frame[3]);
            payload_offset = 4;
        } else if (payload_length == 127) {
            if (frame.size() < 10) {
                throw std::runtime_error("Invalid WebSocket frame: incomplete 64-bit length");
            }
            payload_length = 0;
            for (int i = 0; i < 8; ++i) {
                payload_length = (payload_length << 8) | static_cast<uint8_t>(frame[2 + i]);
            }
            payload_offset = 10;
        }
        
        if (header.mask) {
            if (frame.size() < payload_offset + 4) {
                throw std::runtime_error("Invalid WebSocket frame: incomplete masking key");
            }
            std::copy(frame.data() + payload_offset, frame.data() + payload_offset + 4, 
                    header.masking_key);
            payload_offset += 4;
        }
        
        if (frame.size() < payload_offset + payload_length) {
            throw std::runtime_error("Invalid WebSocket frame: incomplete payload");
        }
        
        std::string payload = frame.substr(payload_offset, payload_length);
        if (header.mask) {
            // Unmask payload
            for (uint64_t i = 0; i < payload_length; ++i) {
                payload[i] ^= header.masking_key[i % 4];
            }
        }
        
        return {header.opcode, payload};
    }
    
    static std::string generate_accept_key(const std::string& client_key) {
        const std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        
        unsigned char md[SHA_DIGEST_LENGTH];
        SHA_CTX ctx;
        SHA1_Init(&ctx);
        SHA1_Update(&ctx, client_key.data(), client_key.size());
        SHA1_Update(&ctx, guid.data(), guid.size());
        SHA1_Final(md, &ctx);
        
        return base64_encode(md, SHA_DIGEST_LENGTH);
    }
    
    static std::unordered_map<std::string, std::string> parse_headers(const std::string& header_data) {
        std::unordered_map<std::string, std::string> headers;
        size_t pos = 0;
        
        while (pos < header_data.size()) {
            size_t line_end = header_data.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            
            std::string line = header_data.substr(pos, line_end - pos);
            if (line.empty()) break; // End of headers
            
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 2); // Skip space after colon
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                headers[key] = value;
            }
            pos = line_end + 2;
        }
        
        return headers;
    }

private:
    static std::string base64_encode(unsigned char* bytes_to_encode, unsigned int in_len) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO* sink = BIO_new(BIO_s_mem());
        b64 = BIO_push(b64, sink);
        BIO_write(b64, bytes_to_encode, in_len);
        BIO_flush(b64);
        BUF_MEM* bptr = nullptr;
        BIO_get_mem_ptr(b64, &bptr);
        std::string result(bptr->data, bptr->length - 1); // Exclude final null
        BIO_free_all(b64);
        return result;
    }
};

// WebSocket client
class WebSocketClient {
public:
    using MessageHandler = std::function<void(WebSocketOpcode, const std::string&)>;
    
    explicit WebSocketClient(MessageHandler handler = {}) : handler_(handler) {}
    
    void connect(const std::string& url) {
        is_connected_ = true;
        handshake_complete_ = false;
        
        // Generate WebSocket key
        std::string key = generate_websocket_key();
        
        // Simulate connection
        std::string request = 
            "GET " + url + " HTTP/1.1\r\n"
            "Host: " + url + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + key + "\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";
        
        // Simulate handshake response
        received_data_ = request;
        parse_response(received_data_);
        
        handshake_complete_ = extract_accept_key(received_data_) == WebSocketFrame::generate_accept_key(key);
    }
    
    void send_message(const std::string& message, WebSocketOpcode opcode = WebSocketOpcode::TEXT) {
        if (!is_connected_) return;
        
        std::string frame = WebSocketFrame::create_frame(message, opcode);
        if (handler_) {
            handler_(opcode, message);
        }
    }
    
    void receive_message() {
        if (!is_connected_) return;
    }
    
    void close(WebSocketCloseCode code = WebSocketCloseCode::NORMAL_CLOSURE, 
             const std::string& reason = {}) {
        if (is_connected_) {
            std::string close_frame = WebSocketFrame::create_frame(
                std::string(2, '\0') + static_cast<char>((static_cast<uint16_t>(code) >> 8) & 0xFF) +
                static_cast<char>(static_cast<uint16_t>(code) & 0xFF) + reason,
                WebSocketOpcode::CLOSE
            );
            is_connected_ = false;
        }
    }
    
    bool connected() const { return is_connected_; }
    bool handshake_complete() const { return handshake_complete_; }

private:
    std::string generate_websocket_key() {
        char key[24];
        for (int i = 0; i < 24; ++i) {
            key[i] = static_cast<char>('A' + (rand() % 26));
        }
        return std::string(key, 24);
    }
    
    void parse_response(const std::string& response) {
        if (response.find("HTTP/1.1 101 Switching Protocols") != std::string::npos) {
            handshake_complete_ = true;
        }
    }
    
    std::string extract_accept_key(const std::string& response) {
        size_t pos = response.find("Sec-WebSocket-Accept: ");
        if (pos != std::string::npos) {
            pos += 22; // Length of the header
            size_t end = response.find("\r\n", pos);
            if (end != std::string::npos) {
                return response.substr(pos, end - pos);
            }
        }
        return {};
    }
    
    MessageHandler handler_;
    std::string received_data_;
    bool is_connected_ = false;
    bool handshake_complete_ = false;
    std::mutex send_mutex_;
};

// WebSocket connection manager for multiple streams
class WebSocketConnectionManager {
public:
    std::shared_ptr<WebSocketClient> create_connection(const std::string& url, 
                                                     WebSocketClient::MessageHandler handler = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto conn = std::make_shared<WebSocketClient>(handler);
        connections_[url] = conn;
        return conn;
    }
    
    std::shared_ptr<WebSocketClient> get_connection(const std::string& url) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(url);
        if (it != connections_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void close_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [url, conn] : connections_) {
            conn->close();
        }
        connections_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

private:
    std::unordered_map<std::string, std::shared_ptr<WebSocketClient>> connections_;
    mutable std::mutex mutex_;
};

} // namespace hyperion::network::websocket