#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>

namespace hre::net {

struct resource_timing {
    std::string url;
    std::string initiator_type; // "document", "script", "link", "img", etc.

    int64_t start_time = 0;
    int64_t redirect_start = 0;
    int64_t redirect_end = 0;
    int64_t domain_lookup_start = 0;
    int64_t domain_lookup_end = 0;
    int64_t connect_start = 0;
    int64_t connect_end = 0;
    int64_t secure_connection_start = 0;
    int64_t request_start = 0;
    int64_t response_start = 0;
    int64_t response_end = 0;

    uint64_t transfer_size = 0;
    uint64_t encoded_body_size = 0;
    uint64_t decoded_body_size = 0;

    int http_status_code = 0;
    std::string protocol;
    bool from_cache = false;

    std::unordered_map<std::string, std::string> custom_tags;

    resource_timing() = default;

    int64_t duration() const {
        return response_end - start_time;
    }

    int64_t dns_time() const {
        if (domain_lookup_end >= domain_lookup_start)
            return domain_lookup_end - domain_lookup_start;
        return 0;
    }

    int64_t tcp_time() const {
        if (connect_end >= connect_start)
            return connect_end - connect_start;
        return 0;
    }

    int64_t tls_time() const {
        if (connect_end >= secure_connection_start && secure_connection_start > 0)
            return connect_end - secure_connection_start;
        return 0;
    }

    int64_t request_time() const {
        if (response_start >= request_start)
            return response_start - request_start;
        return 0;
    }

    int64_t response_time() const {
        if (response_end >= response_start)
            return response_end - response_start;
        return 0;
    }
};

struct har_entry {
    std::string request_url;
    std::string request_method;
    std::string request_http_version;
    std::unordered_map<std::string, std::string> request_headers;

    int response_status = 0;
    std::string response_status_text;
    std::string response_http_version;
    std::unordered_map<std::string, std::string> response_headers;
    std::string response_content_mime_type;
    uint64_t response_content_size = 0;

    int64_t started_date_time = 0;
    int64_t time = 0;

    // DNS, connect, SSL, wait, receive timings (ms)
    int64_t dns = -1;
    int64_t connect = -1;
    int64_t ssl = -1;
    int64_t wait = -1;
    int64_t receive = -1;

    std::string server_ip_address;
    std::string connection;
    std::string comment;
};

class net_metrics_collector {
public:
    net_metrics_collector();
    ~net_metrics_collector();

    uint64_t start_resource(const std::string& url, const std::string& initiator_type);

    void end_resource(uint64_t resource_id);
    void update_resource(uint64_t resource_id, const std::string& field, int64_t value);
    void set_resource_info(uint64_t resource_id, const std::string& key, const std::string& value);

    resource_timing get_resource(uint64_t resource_id) const;
    std::vector<resource_timing> get_all_resources() const;
    void clear();

    har_entry to_har_entry(const resource_timing& timing) const;
    std::string export_har() const;

    uint64_t total_transfer_size() const;
    uint64_t total_requests() const;
    int64_t page_load_time() const;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace hre::net
