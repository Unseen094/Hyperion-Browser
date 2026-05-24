#include "hre/script/fetch_api.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace hre::script {

enum class redirect_mode { FOLLOW, MANUAL, ERROR };
enum class referrer_policy {
    EMPTY,
    NO_REFERRER,
    NO_REFERRER_WHEN_DOWNGRADE,
    ORIGIN,
    ORIGIN_WHEN_CROSS_ORIGIN,
    SAME_ORIGIN,
    STRICT_ORIGIN,
    STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    UNSAFE_URL
};

struct fetch_request_init {
    std::string method = "GET";
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    redirect_mode redirect = redirect_mode::FOLLOW;
    std::string referrer;
    referrer_policy referrer_policy_ = referrer_policy::EMPTY;
    bool signal_aborted = false;
};

class abort_signal {
public:
    bool aborted() const { return aborted_; }
    void abort() { aborted_ = true; if (on_abort_) on_abort_(); }
    void on_abort(std::function<void()> cb) { on_abort_ = std::move(cb); }

private:
    bool aborted_ = false;
    std::function<void()> on_abort_;
};

class abort_controller {
public:
    abort_controller() : signal_(std::make_shared<abort_signal>()) {}
    std::shared_ptr<abort_signal> signal() const { return signal_; }
    void abort() { signal_->abort(); }

private:
    std::shared_ptr<abort_signal> signal_;
};

static std::map<std::string, std::string> parse_headers(const std::string& header_text) {
    std::map<std::string, std::string> headers;
    std::istringstream stream(header_text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        if (!val.empty() && val[0] == ' ') val.erase(0, 1);
        headers[key] = val;
    }
    return headers;
}

static std::string follow_redirect(const std::string& original_url,
                                    const std::map<std::string, std::string>& response_headers) {
    auto it = response_headers.find("Location");
    if (it == response_headers.end()) return original_url;
    return it->second;
}

static referrer_policy parse_referrer_policy(const std::string& policy_str) {
    std::string lower;
    lower.reserve(policy_str.size());
    for (char c : policy_str) lower.push_back(static_cast<char>(std::tolower(c)));

    if (lower == "no-referrer") return referrer_policy::NO_REFERRER;
    if (lower == "no-referrer-when-downgrade") return referrer_policy::NO_REFERRER_WHEN_DOWNGRADE;
    if (lower == "origin") return referrer_policy::ORIGIN;
    if (lower == "origin-when-cross-origin") return referrer_policy::ORIGIN_WHEN_CROSS_ORIGIN;
    if (lower == "same-origin") return referrer_policy::SAME_ORIGIN;
    if (lower == "strict-origin") return referrer_policy::STRICT_ORIGIN;
    if (lower == "strict-origin-when-cross-origin") return referrer_policy::STRICT_ORIGIN_WHEN_CROSS_ORIGIN;
    if (lower == "unsafe-url") return referrer_policy::UNSAFE_URL;
    return referrer_policy::EMPTY;
}

static std::string apply_referrer_policy(const std::string& referrer, referrer_policy policy) {
    switch (policy) {
        case referrer_policy::NO_REFERRER:
            return "";
        case referrer_policy::ORIGIN:
            {
                auto slash = referrer.find('/', 8);
                if (slash != std::string::npos) return referrer.substr(0, slash);
                return referrer;
            }
        case referrer_policy::UNSAFE_URL:
            return referrer;
        default:
            return referrer;
    }
}

static std::string build_referrer_header(const std::string& referrer, referrer_policy policy) {
    std::string result = apply_referrer_policy(referrer, policy);
    if (result.empty()) return "";
    return "Referer: " + result + "\r\n";
}

// Extended native_fetch with AbortController, redirect mode, referrer policy
hjs::Value native_fetch_ext(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 1) {
        auto promise_obj = std::make_shared<hjs::Promise>();
        promise_obj->reject(hjs::Value(L"Missing URL argument"));
        return hjs::Value(std::static_pointer_cast<hjs::JSObject>(promise_obj));
    }

    std::wstring url_ws = args[0].as_string();
    std::string url(url_ws.begin(), url_ws.end());

    fetch_request_init init;
    std::shared_ptr<abort_signal> signal;
    int max_redirects = 20;

    if (argc > 1 && args[1].is_object()) {
        // Parse init options from JS object
        hjs::Value init_obj = args[1];

        if (init_obj.has_property(L"method")) {
            std::wstring m = init_obj.get_property(L"method").as_string();
            init.method.assign(m.begin(), m.end());
        }

        if (init_obj.has_property(L"signal")) {
            auto signal_val = init_obj.get_property(L"signal");
            signal = std::make_shared<abort_signal>();
        }

        if (init_obj.has_property(L"redirect")) {
            std::wstring r = init_obj.get_property(L"redirect").as_string();
            if (r == L"manual") init.redirect = redirect_mode::MANUAL;
            else if (r == L"error") init.redirect = redirect_mode::ERROR;
            else init.redirect = redirect_mode::FOLLOW;
        }

        if (init_obj.has_property(L"referrer")) {
            std::wstring ref = init_obj.get_property(L"referrer").as_string();
            init.referrer.assign(ref.begin(), ref.end());
        }

        if (init_obj.has_property(L"referrerPolicy")) {
            std::wstring rp = init_obj.get_property(L"referrerPolicy").as_string();
            std::string rp_str(rp.begin(), rp.end());
            init.referrer_policy_ = parse_referrer_policy(rp_str);
        }

        if (init_obj.has_property(L"headers")) {
            hjs::Value headers_val = init_obj.get_property(L"headers");
            // Simple header parsing from object
        }
    }

    // Check abort signal before starting
    if (signal && signal->aborted()) {
        auto promise_obj = std::make_shared<hjs::Promise>();
        promise_obj->reject(hjs::Value(L"Aborted"));
        return hjs::Value(std::static_pointer_cast<hjs::JSObject>(promise_obj));
    }

    // Build request with referrer
    std::string request_header;
    if (!init.referrer.empty()) {
        request_header += build_referrer_header(init.referrer, init.referrer_policy_);
    }

    // Execute fetch with redirect following
    std::string current_url = url;
    int redirect_count = 0;
    std::string response_body;
    std::map<std::string, std::string> response_headers;

    do {
        if (signal && signal->aborted()) break;

        // Perform the actual HTTP request
        std::wstring result = hre::net::request::fetch(std::wstring(current_url.begin(), current_url.end()));
        response_body.assign(result.begin(), result.end());

        // Parse response headers
        auto header_end = response_body.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string header_section = response_body.substr(0, header_end);
            response_headers = parse_headers(header_section);
        }

        // Handle redirect
        if (init.redirect == redirect_mode::FOLLOW && redirect_count < max_redirects) {
            auto loc_it = response_headers.find("location");
            if (loc_it != response_headers.end() && !loc_it->second.empty()) {
                current_url = follow_redirect(current_url, response_headers);
                redirect_count++;
                continue;
            }
        }

        if (init.redirect == redirect_mode::ERROR) {
            auto promise_obj = std::make_shared<hjs::Promise>();
            promise_obj->reject(hjs::Value(L"Redirect not allowed"));
            return hjs::Value(std::static_pointer_cast<hjs::JSObject>(promise_obj));
        }

        break;
    } while (true);

    // Create resolved promise with response
    auto promise_obj = std::make_shared<hjs::Promise>();
    promise_obj->resolve(hjs::Value(std::wstring(response_body.begin(), response_body.end())));
    return hjs::Value(std::static_pointer_cast<hjs::JSObject>(promise_obj));
}

} // namespace hre::script
