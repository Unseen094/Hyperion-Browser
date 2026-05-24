#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include "frames.h"

namespace hyperion::network::http2 {

// HTTP/2 stream state
enum class StreamState : uint8_t {
    IDLE,
    RESERVED_LOCAL,
    RESERVED_REMOTE,
    OPEN,
    HALF_CLOSED_LOCAL,
    HALF_CLOSED_REMOTE,
    CLOSED,
};

// HTTP/2 priority information
struct StreamPriority {
    uint32_t stream_dependency;
    uint8_t weight;
    bool exclusive;
};

// HTTP/2 stream
class Http2Stream {
public:
    Http2Stream(uint32_t stream_id, StreamPriority priority = {0, 16, false})
        : id_(stream_id), state_(StreamState::IDLE), priority_(priority) {}
    
    uint32_t id() const { return id_; }
    StreamState state() const { return state_; }
    void set_state(StreamState state) { state_ = state; }
    
    const StreamPriority& priority() const { return priority_; }
    void set_priority(const StreamPriority& priority) { priority_ = priority; }
    
    void add_data(const std::vector<uint8_t>& data) {
        received_data_.insert(received_data_.end(), data.begin(), data.end());
    }
    
    const std::vector<uint8_t>& data() const { return received_data_; }
    void clear_data() { received_data_.clear(); }
    
    bool is_client_initiated() const {
        return (id_ % 2 == 1);
    }
    
    // Flow control window
    uint32_t window_size() const { return flow_control_window_; }
    void set_window_size(uint32_t size) { flow_control_window_ = size; }
    void consume_window(uint32_t amount) {
        if (amount > flow_control_window_) {
            flow_control_window_ = 0;
        } else {
            flow_control_window_ -= amount;
        }
    }
    
    // Headers management
    const std::vector<uint8_t>& headers() const { return headers_; }
    void set_headers(const std::vector<uint8_t>& headers) { headers_ = headers; }

private:
    uint32_t flow_control_window_ = 65535; // Default window size
    std::vector<uint8_t> headers_;
    uint32_t id_;
    StreamState state_;
    StreamPriority priority_;
    std::vector<uint8_t> received_data_;
    std::vector<uint8_t> headers_;
};

// HTTP/2 connection with stream multiplexing
class Http2ConnectionManager {
public:
    Http2ConnectionManager() : next_stream_id_(1) {} // Start with 1 for client-initiated streams
    
    // Create a new stream
    uint32_t create_stream(StreamPriority priority = {0, 16, false}) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t stream_id = next_stream_id_;
        next_stream_id_ += 2; // Increment by 2 per HTTP/2 spec
        
        streams_[stream_id] = std::make_unique<Http2Stream>(stream_id, priority);
        return stream_id;
    }
    
    // Get a stream by ID
    Http2Stream* get_stream(uint32_t stream_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream_id);
        if (it == streams_.end()) {
            return nullptr;
        }
        return it->second.get();
    }
    
    // Remove a stream
    void remove_stream(uint32_t stream_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        streams_.erase(stream_id);
    }
    
    // Update stream state
    void update_stream_state(uint32_t stream_id, StreamState new_state) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            it->second->set_state(new_state);
        }
    }
    
    // Update stream priority
    void update_stream_priority(uint32_t stream_id, const StreamPriority& priority) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            it->second->set_priority(priority);
        }
    }
    
    // Add data to a stream
    void add_stream_data(uint32_t stream_id, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            it->second->add_data(data);
        }
    }
    
    // Check if stream exists
    bool has_stream(uint32_t stream_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return streams_.find(stream_id) != streams_.end();
    }
    
    // Count active streams
    size_t active_stream_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& [id, stream] : streams_) {
            if (stream->state() != StreamState::CLOSED) {
                count++;
            }
        }
        return count;
    }
    
    // Close all streams
    void close_all_streams() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, stream] : streams_) {
            if (stream->state() != StreamState::CLOSED) {
                stream->set_state(StreamState::CLOSED);
            }
        }
    }
    
    // Reset all streams with error
    void reset_all_streams(ErrorCode error) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, stream] : streams_) {
            if (stream->state() != StreamState::CLOSED) {
                auto frame = std::make_unique<RstStreamFrame>();
                FrameHeader header;
                header.type = static_cast<uint8_t>(FrameType::RST_STREAM);
                header.stream_id = id;
                header.length = 4;
                header.flags = 0;
                frame->set_header(header);
                frame->set_error_code(error);
                // In real implementation: queue frame for sending
            }
        }
    }
    
    // Get stream priority
    StreamPriority get_stream_priority(uint32_t stream_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            return it->second->priority();
        }
        return {0, 16, false};
    }
    
    // Get all active streams
    std::vector<Http2Stream*> get_active_streams() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Http2Stream*> result;
        for (const auto& [id, stream] : streams_) {
            if (stream->state() != StreamState::CLOSED) {
                result.push_back(stream.get());
            }
        }
        return result;
    }
    
    // Reserve a push stream (server push)
    uint32_t reserve_push_stream(uint32_t parent_stream_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Push streams use even IDs (1, 3, 5, ... from server)
        uint32_t stream_id = (parent_stream_id % 2 == 1) ? parent_stream_id + 1 : parent_stream_id;
        while (streams_.find(stream_id) != streams_.end()) {
            stream_id += 2;
        }
        
        StreamPriority priority = {
            .stream_dependency = parent_stream_id,
            .weight = 16,
            .exclusive = false
        };
        
        streams_[stream_id] = std::make_unique<Http2Stream>(stream_id, priority);
        streams_[stream_id]->set_state(StreamState::RESERVED_REMOTE);
        return stream_id;
    }
    
private:
    uint32_t next_stream_id_;
    std::unordered_map<uint32_t, std::unique_ptr<Http2Stream>> streams_;
    mutable std::mutex mutex_;
};

} // namespace hyperion::network::http2