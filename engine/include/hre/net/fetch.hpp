#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace hre::net {

enum class cors_mode { Navigate, SameOrigin, NoCors, Cors };
enum class credentials_mode { Omit, SameOrigin, Include };
enum class redirect_mode { Follow, Error, Manual };
enum class referrer_policy { NoReferrer, NoReferrerWhenDowngrade, Origin, OriginWhenCrossOrigin, UnsafeUrl };

struct fetch_request {
    std::string method = "GET";
    std::string url;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    cors_mode cors = cors_mode::Cors;
    credentials_mode credentials = credentials_mode::SameOrigin;
    redirect_mode redirect = redirect_mode::Follow;
    referrer_policy referrer = referrer_policy::NoReferrerWhenDowngrade;
    int timeout_ms = 30000;
    bool keepalive = false;
    bool prefer_h2 = true;
};

struct fetch_response {
    int status_code = 0;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::string url;
    bool ok = false;
    bool redirected = false;
    std::string type; // "basic", "cors", "opaque", "opaqueredirect"
    int timeout_ms = 0;

    std::string text() const;
    std::map<std::string, std::string> json() const;
};

class fetch_api {
public:
    fetch_api() = default;

    void fetch(const fetch_request& req, std::function<void(const fetch_response&)> callback);
    void fetch_async(const fetch_request& req, std::function<void(const fetch_response&)> callback);

    // Abort controller
    void abort(const std::string& request_id);

    // Pre-processing
    static fetch_request process_request(const fetch_request& req);
    static fetch_response process_response(const fetch_response& resp, const fetch_request& req);

private:
    struct pending_request {
        fetch_request request;
        std::function<void(const fetch_response&)> callback;
        bool aborted = false;
        std::string id;
    };

    std::vector<pending_request> pending_;

    void do_fetch(int index);
    void complete_fetch(int index, const fetch_response& response);
    std::string generate_id();
};

} // namespace hre::net
