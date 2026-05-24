#include "hre/net/net_metrics.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>

namespace hre::net {

class net_metrics_collector::impl {
public:
    std::unordered_map<uint64_t, resource_timing> resources_;
    uint64_t next_id_ = 1;
    mutable std::mutex mtx_;

    uint64_t start(const std::string& url, const std::string& initiator_type) {
        std::lock_guard<std::mutex> lock(mtx_);
        uint64_t id = next_id_++;
        resource_timing rt;
        rt.url = url;
        rt.initiator_type = initiator_type;
        rt.start_time = now_ms();
        resources_[id] = rt;
        return id;
    }

    void end(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            it->second.response_end = now_ms();
        }
    }

    void update(uint64_t id, const std::string& field, int64_t value) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = resources_.find(id);
        if (it == resources_.end()) return;

        auto& rt = it->second;
        if (field == "redirect_start") rt.redirect_start = value;
        else if (field == "redirect_end") rt.redirect_end = value;
        else if (field == "domain_lookup_start") rt.domain_lookup_start = value;
        else if (field == "domain_lookup_end") rt.domain_lookup_end = value;
        else if (field == "connect_start") rt.connect_start = value;
        else if (field == "connect_end") rt.connect_end = value;
        else if (field == "secure_connection_start") rt.secure_connection_start = value;
        else if (field == "request_start") rt.request_start = value;
        else if (field == "response_start") rt.response_start = value;
        else if (field == "response_end") rt.response_end = value;
        else if (field == "transfer_size") rt.transfer_size = static_cast<uint64_t>(value);
        else if (field == "encoded_body_size") rt.encoded_body_size = static_cast<uint64_t>(value);
        else if (field == "decoded_body_size") rt.decoded_body_size = static_cast<uint64_t>(value);
        else if (field == "http_status_code") rt.http_status_code = static_cast<int>(value);
        else if (field == "from_cache") rt.from_cache = (value != 0);
    }

    void set_info(uint64_t id, const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            if (key == "protocol") it->second.protocol = value;
            else it->second.custom_tags[key] = value;
        }
    }

    resource_timing get(uint64_t id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = resources_.find(id);
        return it != resources_.end() ? it->second : resource_timing{};
    }

    std::vector<resource_timing> all() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<resource_timing> result;
        for (const auto& [id, rt] : resources_) {
            result.push_back(rt);
        }
        return result;
    }

    void clear_all() {
        std::lock_guard<std::mutex> lock(mtx_);
        resources_.clear();
        next_id_ = 1;
    }

    uint64_t total_transfer() const {
        std::lock_guard<std::mutex> lock(mtx_);
        uint64_t total = 0;
        for (const auto& [id, rt] : resources_) {
            total += rt.transfer_size;
        }
        return total;
    }

    int64_t now_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::string export_har() {
        std::lock_guard<std::mutex> lock(mtx_);

        std::ostringstream json;
        json << "{\n";
        json << "  \"log\": {\n";
        json << "    \"version\": \"1.2\",\n";
        json << "    \"creator\": {\n";
        json << "      \"name\": \"HyperionBrowser\",\n";
        json << "      \"version\": \"0.1.0\"\n";
        json << "    },\n";
        json << "    \"entries\": [\n";

        bool first = true;
        for (const auto& [id, rt] : resources_) {
            if (!first) json << ",\n";
            first = false;

            har_entry he = to_har_entry(rt);
            json << "      {\n";
            json << "        \"request\": {\n";
            json << "          \"method\": \"" << he.request_method << "\",\n";
            json << "          \"url\": \"" << escape_json(he.request_url) << "\",\n";
            json << "          \"httpVersion\": \"" << he.request_http_version << "\",\n";
            json << "          \"headers\": [\n";
            bool hfirst = true;
            for (const auto& [k, v] : he.request_headers) {
                if (!hfirst) json << ",\n";
                hfirst = false;
                json << "            { \"name\": \"" << escape_json(k)
                     << "\", \"value\": \"" << escape_json(v) << "\" }";
            }
            json << "\n          ],\n";
            json << "          \"headersSize\": -1,\n";
            json << "          \"bodySize\": -1\n";
            json << "        },\n";
            json << "        \"response\": {\n";
            json << "          \"status\": " << he.response_status << ",\n";
            json << "          \"statusText\": \"" << escape_json(he.response_status_text) << "\",\n";
            json << "          \"httpVersion\": \"" << he.response_http_version << "\",\n";
            json << "          \"headers\": [\n";
            hfirst = true;
            for (const auto& [k, v] : he.response_headers) {
                if (!hfirst) json << ",\n";
                hfirst = false;
                json << "            { \"name\": \"" << escape_json(k)
                     << "\", \"value\": \"" << escape_json(v) << "\" }";
            }
            json << "\n          ],\n";
            json << "          \"content\": {\n";
            json << "            \"size\": " << he.response_content_size << ",\n";
            json << "            \"mimeType\": \"" << escape_json(he.response_content_mime_type) << "\"\n";
            json << "          },\n";
            json << "          \"headersSize\": -1,\n";
            json << "          \"bodySize\": " << he.response_content_size << "\n";
            json << "        },\n";
            json << "        \"cache\": {},\n";
            json << "        \"timings\": {\n";
            json << "          \"dns\": " << he.dns << ",\n";
            json << "          \"connect\": " << he.connect << ",\n";
            json << "          \"ssl\": " << he.ssl << ",\n";
            json << "          \"wait\": " << he.wait << ",\n";
            json << "          \"receive\": " << he.receive << "\n";
            json << "        },\n";
            json << "        \"time\": " << he.time << ",\n";
            json << "        \"startedDateTime\": \"" << format_time(he.started_date_time) << "\",\n";
            json << "        \"connection\": \"" << he.connection << "\",\n";
            json << "        \"serverIPAddress\": \"" << he.server_ip_address << "\"\n";
            json << "      }";
        }

        json << "\n    ]\n";
        json << "  }\n";
        json << "}\n";
        return json.str();
    }

    static std::string escape_json(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    }

    static std::string format_time(int64_t ms) {
        // Convert ms since epoch to ISO 8601
        auto dur = std::chrono::milliseconds(ms);
        auto tp = std::chrono::time_point<std::chrono::system_clock,
                    std::chrono::milliseconds>(dur);
        auto tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = {};
        gmtime_s(&tm, &tt);

        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
        ss << "." << std::setw(3) << std::setfill('0') << (ms % 1000);
        ss << "Z";
        return ss.str();
    }

    har_entry to_har_entry(const resource_timing& rt) {
        har_entry he;
        he.request_url = rt.url;
        he.response_status = rt.http_status_code;
        he.started_date_time = rt.start_time;
        he.time = rt.duration();

        he.dns = rt.dns_time();
        he.connect = rt.tcp_time();
        he.ssl = rt.tls_time();
        he.wait = rt.request_time();
        he.receive = rt.response_time();

        he.response_content_size = rt.decoded_body_size;
        he.server_ip_address = "";
        he.connection = rt.protocol;
        return he;
    }
};

net_metrics_collector::net_metrics_collector() : pimpl_(std::make_unique<impl>()) {}
net_metrics_collector::~net_metrics_collector() = default;

uint64_t net_metrics_collector::start_resource(const std::string& url, const std::string& initiator_type) {
    return pimpl_->start(url, initiator_type);
}

void net_metrics_collector::end_resource(uint64_t resource_id) {
    pimpl_->end(resource_id);
}

void net_metrics_collector::update_resource(uint64_t resource_id, const std::string& field, int64_t value) {
    pimpl_->update(resource_id, field, value);
}

void net_metrics_collector::set_resource_info(uint64_t resource_id, const std::string& key, const std::string& value) {
    pimpl_->set_info(resource_id, key, value);
}

resource_timing net_metrics_collector::get_resource(uint64_t resource_id) const {
    return pimpl_->get(resource_id);
}

std::vector<resource_timing> net_metrics_collector::get_all_resources() const {
    return pimpl_->all();
}

void net_metrics_collector::clear() {
    pimpl_->clear_all();
}

har_entry net_metrics_collector::to_har_entry(const resource_timing& timing) const {
    return pimpl_->to_har_entry(timing);
}

std::string net_metrics_collector::export_har() const {
    return const_cast<net_metrics_collector::impl*>(pimpl_.get())->export_har();
}

uint64_t net_metrics_collector::total_transfer_size() const {
    return pimpl_->total_transfer();
}

uint64_t net_metrics_collector::total_requests() const {
    return pimpl_->all().size();
}

int64_t net_metrics_collector::page_load_time() const {
    auto resources = pimpl_->all();
    if (resources.empty()) return 0;
    auto min_start = std::min_element(resources.begin(), resources.end(),
        [](const resource_timing& a, const resource_timing& b) {
            return a.start_time < b.start_time;
        });
    auto max_end = std::max_element(resources.begin(), resources.end(),
        [](const resource_timing& a, const resource_timing& b) {
            return a.response_end < b.response_end;
        });
    return max_end->response_end - min_start->start_time;
}

} // namespace hre::net
