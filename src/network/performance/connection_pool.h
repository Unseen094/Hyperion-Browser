#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <chrono>
#include "../udp/udp_transport.h"

namespace hyperion::network::performance {

// HTTP/2 connection holder for pooling
class Http2ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<hyperion::network::http2::Http2ConnectionManager>;
    
    // Get or create connection to host
    ConnectionPtr get_connection(const std::string& host, uint16_t port) {
        std::lock_guard<std::mutex> lock(mutex_);
        ConnectionKey key = {host, port};
        
        auto it = connections_.find(key);
        if (it != connections_.end() && it->second->active_stream_count() > 0) {
            // Connection is active, return it
            return it->second;
        }
        
        // Create new connection
        auto sock = hyperion::network::udp::UdpTransport::create_socket();
        auto conn_mgr = std::make_shared<hyperion::network::http2::Http2ConnectionManager>();
        
        // In real implementation: perform connection establishment and settings exchange
        // For now: simulate active connection
        connections_[key] = conn_mgr;
        last_used_[key] = std::chrono::steady_clock::now();
        
        return conn_mgr;
    }
    
    // Release connection back to pool
    void release_connection(const std::string& host, uint16_t port) {
        std::lock_guard<std::mutex> lock(mutex_);
        ConnectionKey key = {host, port};
        last_used_[key] = std::chrono::steady_clock::now();
    }
    
    // Clean up stale connections (>30 seconds idle)
    void cleanup_stale_connections() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto it = connections_.begin();
        
        while (it != connections_.end()) {
            auto last_it = last_used_.find(it->first);
            if (last_it != last_used_.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_it->second).count();
                if (elapsed > 30) {
                    it = connections_.erase(it);
                    last_used_.erase(last_it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
    
    // Get pooled or established connection count
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }
    
    // Invalidate connection with error
    void invalidate_connection(const std::string& host, uint16_t port) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(ConnectionKey{host, port});
        last_used_.erase(ConnectionKey{host, port});
    }

private:
    struct ConnectionKey {
        std::string host;
        uint16_t port;
        
        bool operator==(const ConnectionKey& other) const {
            return host == other.host && port == other.port;
        }
    };
    
    struct ConnectionKeyHash {
        size_t operator()(const ConnectionKey& key) const {
            return std::hash<std::string>()(key.host) ^ std::hash<uint16_t>()(key.port);
        }
    };
    
    std::unordered_map<ConnectionKey, ConnectionPtr, ConnectionKeyHash> connections_;
    std::unordered_map<ConnectionKey, std::chrono::steady_clock::time_point, ConnectionKeyHash> last_used_;
    mutable std::mutex mutex_;
};

// DNS cache for performance
class DnsCache {
public:
    void add_entry(const std::string& hostname, const std::string& ip, uint16_t port = 80) {
        std::lock_guard<std::mutex> lock(mutex_);
        Key key = {hostname, port};
        cache_[key] = ip;
        timestamps_[key] = std::chrono::steady_clock::now();
    }
    
    std::string lookup(const std::string& hostname, uint16_t port = 80) {
        std::lock_guard<std::mutex> lock(mutex_);
        Key key = {hostname, port};
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - timestamps_[key]
            ).count();
            if (elapsed < 3600) { // 1 hour TTL
                return it->second;
            }
        }
        return {};
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
    void cleanup_old_entries() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto it = cache_.begin();
        
        while (it != cache_.end()) {
            auto ts_it = timestamps_.find(it->first);
            if (ts_it != timestamps_.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - ts_it->second).count();
                if (elapsed > 3600) {
                    it = cache_.erase(it);
                    timestamps_.erase(ts_it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }

private:
    using Key = std::pair<std::string, uint16_t>;
    std::unordered_map<Key, std::string> cache_;
    std::unordered_map<Key, std::chrono::steady_clock::time_point> timestamps_;
    mutable std::mutex mutex_;
};

// Network bandwidth monitor and throttler
class BandwidthMonitor {
public:
    void add_bytes_sent(size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        bytes_sent_ += bytes;
        last_update_ = std::chrono::steady_clock::now();
    }
    
    void add_bytes_received(size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        bytes_received_ += bytes;
        last_update_ = std::chrono::steady_clock::now();
    }
    
    // Get current bandwidth usage (bytes per second)
    std::pair<size_t, size_t> get_bandwidth() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_).count();
        
        if (elapsed == 0) return {bytes_sent_, bytes_received_};
        
        size_t sent_per_sec = bytes_sent_ / (elapsed + 1);
        size_t recv_per_sec = bytes_received_ / (elapsed + 1);
        
        // Reset counters after reading
        bytes_sent_ = 0;
        bytes_received_ = 0;
        last_update_ = now;
        
        return {sent_per_sec, recv_per_sec};
    }
    
    void set_max_bandwidth(size_t max_sent, size_t max_received) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_bandwidth_sent_ = max_sent;
        max_bandwidth_received_ = max_received;
    }
    
    bool would_exceed_limit(size_t proposed_bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_).count();
        
        size_t current_rate = (bytes_sent_ + proposed_bytes) / (elapsed + 1);
        if (max_bandwidth_sent_ > 0 && current_rate > max_bandwidth_sent_) {
            return true;
        }
        return false;
    }

private:
    size_t bytes_sent_ = 0;
    size_t bytes_received_ = 0;
    size_t max_bandwidth_sent_ = 0;
    size_t max_bandwidth_received_ = 0;
    std::chrono::steady_clock::time_point last_update_ = std::chrono::steady_clock::now();
    mutable std::mutex mutex_;
};

} // namespace hyperion::network::performance