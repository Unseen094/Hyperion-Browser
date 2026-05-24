#pragma once

#include "frames.h"
#include <vector>
#include <memory>
#include <stdexcept>

namespace hyperion::network::http2 {

// HTTP/2 frame parser
class FrameParser {
public:
    // Parse raw frame data into a Frame object
    template<typename T>
    static std::unique_ptr<T> parse_frame(const std::vector<uint8_t>& data) {
        std::unique_ptr<T> frame = std::make_unique<T>();
        frame->parse(data);
        return frame;
    }
    
    // Parse raw frame data without knowing the type
    static std::unique_ptr<Frame> parse_any_frame(const std::vector<uint8_t>& data) {
        if (data.size() < 9) {
            throw std::runtime_error("Frame too short: insufficient for header");
        }
        
        FrameHeader header;
        header.length = (static_cast<uint32_t>(data[0]) << 16) |
                       (static_cast<uint32_t>(data[1]) << 8) |
                       static_cast<uint32_t>(data[2]);
        FrameType type = static_cast<FrameType>(data[3]);
        uint8_t flags = data[4];
        header.stream_id = (static_cast<uint32_t>(data[5]) << 24) |
                          (static_cast<uint32_t>(data[6]) << 16) |
                          (static_cast<uint32_t>(data[7]) << 8) |
                          static_cast<uint32_t>(data[8]);
        
        // Check if more data needed
        size_t expected_size = 9 + header.length;
        if (data.size() < expected_size) {
            throw std::runtime_error("Incomplete frame: expected " + std::to_string(expected_size) + 
                                  " bytes, got " + std::to_string(data.size()));
        }
        
        // Extract payload
        std::vector<uint8_t> payload;
        if (header.length > 0) {
            payload.resize(header.length);
            std::copy(data.begin() + 9, data.begin() + 9 + header.length, payload.begin());
        }
        
        // Dispatch to appropriate frame type
        switch (type) {
            case FrameType::DATA:
                return parse_data_frame(data);
            case FrameType::HEADERS:
                return parse_headers_frame(data);
            case FrameType::SETTINGS:
                return parse_settings_frame(data);
            case FrameType::PRIORITY:
                return parse_priority_frame(data);
            case FrameType::RST_STREAM:
                return parse_rst_stream_frame(data);
            case FrameType::PUSH_PROMISE:
                return parse_push_promise_frame(data);
            case FrameType::PING:
                return parse_ping_frame(data);
            case FrameType::GOAWAY:
                return parse_goaway_frame(data);
            case FrameType::WINDOW_UPDATE:
                return parse_window_update_frame(data);
            case FrameType::CONTINUATION:
                return parse_continuation_frame(data);
            default:
                throw std::runtime_error("Unknown frame type: " + std::to_string(static_cast<uint8_t>(type)));
        }
    }
    
    // Validate frame header
    static bool validate_header(const FrameHeader& header) {
        // Stream ID must be non-zero for frames that aren't connection-controlled
        if (header.stream_id == 0) {
            // DATA frame must have non-zero stream ID
            if (header.type == static_cast<uint8_t>(FrameType::DATA)) {
                return false;
            }
        }
        
        // Length must not exceed max frame size (default: 16384)
        if (header.length > 16384) {
            return false;
        }
        
        return true;
    }

private:
    static std::unique_ptr<Frame> parse_data_frame(const std::vector<uint8_t>& data) {
        auto frame = std::make_unique<DataFrame>();
        frame->parse(data);
        return frame;
    }
    
    static std::unique_ptr<Frame> parse_headers_frame(const std::vector<uint8_t>& data) {
        auto frame = std::make_unique<HeadersFrame>();
        frame->parse(data);
        return frame;
    }
    
    static std::unique_ptr<Frame> parse_settings_frame(const std::vector<uint8_t>& data) {
        auto frame = std::make_unique<SettingsFrame>();
        frame->parse(data);
        return frame;
    }
    
    static std::unique_ptr<Frame> parse_priority_frame(const std::vector<uint8_t>& data) {
        // Placeholder for Phase 11
        auto frame = std::make_unique<PriorityFrame>();
        frame->parse(data);
        return frame;
    }
    
    static std::unique_ptr<Frame> parse_rst_stream_frame(const std::vector<uint8_t>& data) {
        // Placeholder for Phase 11
        auto frame = std::make_unique<RstStreamFrame>();
        frame->parse(data);
        return frame;
    }
    
    static std::unique_ptr<Frame> parse_push_promise_frame(const std::vector<uint8_t>& data) {
        // Placeholder for Phase 11
        throw std::runtime_error("Push Promise frame not implemented in Phase 11");
    }
    
    static std::unique_ptr<Frame> parse_ping_frame(const std::vector<uint8_t>& data) {
        // Placeholder for Phase 11
        throw std::runtime_error("Ping frame not implemented in Phase 11");
    }
    
    static std::unique_ptr<Frame> parse_goaway_frame(const std::vector<uint8_t>& data) {
        // Placeholder for Phase 11
        throw std::runtime_error("GoAway frame not implemented in Phase 11");
    }
    
    static std::unique_ptr<Frame> parse_window_update_frame(const std::vector<uint8_t>& data) {
        // Placeholder for Phase 11
        throw std::runtime_error("Window Update frame not implemented in Phase 11");
    }
    
    static std::unique_ptr<Frame> parse_continuation_frame(const std::vector<uint8_t>& data) {
        // Placeholder for Phase 11
        throw std::runtime_error("Continuation frame not implemented in Phase 11");
    }
};

// HTTP/2 connection handler (phase 1 implementation)
class Http2Connection {
public:
    Http2Connection() = default;
    
    // Parse incoming data and return frames
    std::vector<std::unique_ptr<Frame>> receive(const std::vector<uint8_t>& data) {
        frames_.clear();
        buffer_.insert(buffer_.end(), data.begin(), data.end());
        
        while (!buffer_.empty()) {
            if (buffer_.size() < 9) {
                break; // Need more data for header
            }
            
            // Parse header (9 bytes)
            uint32_t length = (static_cast<uint32_t>(buffer_[0]) << 16) |
                            (static_cast<uint32_t>(buffer_[1]) << 8) |
                            static_cast<uint32_t>(buffer_[2]);
            FrameType type = static_cast<FrameType>(buffer_[3]);
            uint8_t flags = buffer_[4];
            uint32_t stream_id = (static_cast<uint32_t>(buffer_[5]) << 24) |
                               (static_cast<uint32_t>(buffer_[6]) << 16) |
                               (static_cast<uint32_t>(buffer_[7]) << 8) |
                               static_cast<uint32_t>(buffer_[8]);
            
            size_t frame_size = 9 + length;
            if (buffer_.size() < frame_size) {
                break; // Incomplete frame
            }
            
            // Extract frame data
            std::vector<uint8_t> frame_data(buffer_.begin(), buffer_.begin() + frame_size);
            buffer_.erase(buffer_.begin(), buffer_.begin() + frame_size);
            
            // Parse and store frame
            try {
                auto frame = FrameParser::parse_any_frame(frame_data);
                frames_.push_back(std::move(frame));
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Frame parsing failed: ") + e.what());
            }
        }
        
        return std::move(frames_);
    }
    
    // Send data as frames
    std::unique_ptr<Frame> send_data(uint32_t stream_id, const std::vector<uint8_t>& payload) {
        auto frame = std::make_unique<DataFrame>();
        FrameHeader header;
        header.type = static_cast<uint8_t>(FrameType::DATA);
        header.flags = 0;
        header.stream_id = stream_id;
        header.length = static_cast<uint32_t>(payload.size());
        frame->set_header(header);
        frame->set_payload(payload);
        return frame;
    }
    
    std::unique_ptr<Frame> send_headers(uint32_t stream_id, const std::vector<uint8_t>& headers) {
        auto frame = std::make_unique<HeadersFrame>();
        FrameHeader header;
        header.type = static_cast<uint8_t>(FrameType::HEADERS);
        header.flags = 0;
        header.stream_id = stream_id;
        header.length = static_cast<uint32_t>(headers.size());
        frame->set_header(header);
        frame->set_headers(headers);
        return frame;
    }
    
    std::unique_ptr<Frame> send_settings() {
        auto frame = std::make_unique<SettingsFrame>();
        FrameHeader header;
        header.type = static_cast<uint8_t>(FrameType::SETTINGS);
        header.flags = 0;
        header.stream_id = 0; // Connection-level
        header.length = 0;
        frame->set_header(header);
        return frame;
    }
    
    std::vector<uint8_t> frame_to_bytes(const Frame& frame) {
        return frame.serialize();
    }

private:
    std::vector<uint8_t> buffer_;
    std::vector<std::unique_ptr<Frame>> frames_;
};

} // namespace hyperion::network::http2