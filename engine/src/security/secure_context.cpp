#include <hre/security/secure_context.hpp>
#include <algorithm>
#include <sstream>

namespace hre::security {

secure_context_manager::secure_context_manager() = default;
secure_context_manager::~secure_context_manager() = default;

bool secure_context_manager::is_secure_context(const std::wstring& url) const {
    secure_context_type type = classify_context(url);
    return type != secure_context_type::INSECURE;
}

bool secure_context_manager::is_potentially_trustworthy(const std::wstring& url) const {
    secure_context_type type = classify_context(url);
    return type == secure_context_type::CRYPTO ||
           type == secure_context_type::LOCALHOST ||
           type == secure_context_type::FILE ||
           type == secure_context_type::EXTENSION ||
           type == secure_context_type::PRIVILEGED;
}

secure_context_type secure_context_manager::classify_context(const std::wstring& url) const {
    size_t scheme_end = url.find(L"://");
    if (scheme_end == std::wstring::npos) {
        if (url.find(L"about:") == 0 || url.find(L"data:") == 0 || url.find(L"blob:") == 0) {
            return secure_context_type::PRIVILEGED;
        }
        return secure_context_type::INSECURE;
    }

    std::wstring scheme = url.substr(0, scheme_end);
    if (scheme == L"https" || scheme == L"wss") return secure_context_type::CRYPTO;
    if (scheme == L"http" || scheme == L"ws") {
        size_t host_start = scheme_end + 3;
        size_t host_end = url.find_first_of(L":/?#", host_start);
        std::wstring host = url.substr(host_start, host_end - host_start);

        if (is_localhost(host) || is_loopback(host)) return secure_context_type::LOCALHOST;

        if (m_allowed_insecure_origins.empty()) return secure_context_type::INSECURE;
        for (const auto& allowed : m_allowed_insecure_origins) {
            if (url.find(allowed) == 0) return secure_context_type::PRIVILEGED;
        }
        return secure_context_type::INSECURE;
    }
    if (scheme == L"file") return secure_context_type::FILE;
    if (scheme == L"chrome-extension" || scheme == L"moz-extension") return secure_context_type::EXTENSION;
    return secure_context_type::INSECURE;
}

bool secure_context_manager::is_feature_allowed_in_context(const std::wstring& url,
                                                            const std::wstring& feature_name) const {
    if (!secure_features::requires_secure_context(feature_name)) return true;
    return is_secure_context(url);
}

bool secure_context_manager::is_powerful_feature_allowed(const std::wstring& url) const {
    return is_secure_context(url);
}

bool secure_context_manager::is_upgrade_required(const std::wstring& url) const {
    if (!m_https_only) return false;
    size_t scheme_end = url.find(L"://");
    if (scheme_end == std::wstring::npos) return false;
    std::wstring scheme = url.substr(0, scheme_end);
    return scheme == L"http" && !is_potentially_trustworthy(url);
}

std::wstring secure_context_manager::upgrade_to_https(const std::wstring& url) const {
    if (url.find(L"http://") == 0) {
        return L"https://" + url.substr(7);
    }
    if (url.find(L"ws://") == 0) {
        return L"wss://" + url.substr(5);
    }
    return url;
}

secure_context_manager::mixed_content_type
secure_context_manager::classify_mixed_content(const std::wstring& request_url) const {
    size_t scheme_end = request_url.find(L"://");
    if (scheme_end == std::wstring::npos) return mixed_content_type::NONE;
    std::wstring scheme = request_url.substr(0, scheme_end);

    if (scheme == L"https" || scheme == L"wss") return mixed_content_type::NONE;

    if (scheme == L"image" || scheme == L"video" || scheme == L"audio") {
        return mixed_content_type::OPTIONALLY_BLOCKABLE;
    }

    return mixed_content_type::BLOCKABLE;
}

bool secure_context_manager::should_block_mixed_content(const std::wstring& request_url,
                                                         mixed_content_type type) const {
    if (!m_block_mixed) return false;
    return type == mixed_content_type::BLOCKABLE;
}

bool secure_context_manager::should_upgrade_mixed_content(const std::wstring& request_url) const {
    return m_auto_upgrade;
}

void secure_context_manager::add_insecure_origin_allowed(const std::wstring& origin) {
    if (std::find(m_allowed_insecure_origins.begin(), m_allowed_insecure_origins.end(), origin) ==
        m_allowed_insecure_origins.end()) {
        m_allowed_insecure_origins.push_back(origin);
    }
}

bool secure_context_manager::is_insecure_origin_allowed(const std::wstring& origin) const {
    return std::find(m_allowed_insecure_origins.begin(), m_allowed_insecure_origins.end(), origin) !=
           m_allowed_insecure_origins.end();
}

secure_context_manager::security_level secure_context_manager::get_security_level(const std::wstring& url) const {
    if (is_secure_context(url)) {
        if (classify_context(url) == secure_context_type::CRYPTO) return SECURE;
        return WARNING;
    }
    if (is_potentially_trustworthy(url)) return WARNING;
    return INSECURE;
}

std::wstring secure_context_manager::get_security_icon(const std::wstring& url) const {
    switch (get_security_level(url)) {
        case SECURE: return L"🔒";
        case WARNING: return L"⚠️";
        case INSECURE: return L"⚠️";
        case DANGEROUS: return L"🚫";
    }
    return L"";
}

bool secure_context_manager::is_localhost(const std::wstring& host) const {
    return host == L"localhost" || host == L"127.0.0.1" || host == L"::1";
}

bool secure_context_manager::is_loopback(const std::wstring& host) const {
    if (host == L"localhost" || host == L"127.0.0.1" || host == L"::1") return true;
    if (host.find(L"127.") == 0) return true;
    return false;
}

// ---- secure_features ------------------------------------------------------

namespace secure_features {

bool requires_secure_context(const std::wstring& feature_name) {
    static const std::vector<std::wstring> powerful_features = {
        L"geolocation", L"notifications", L"push", L"midi",
        L"camera", L"microphone", L"speaker", L"screen_capture",
        L"clipboard_read", L"clipboard_write", L"payment",
        L"credentials", L"display_capture", L"web_authentication",
        L"serial", L"usb", L"bluetooth", L"nfc",
        L"storage_persist", L"ambient_light_sensor", L"proximity_sensor",
        L"accelerometer", L"gyroscope", L"magnetometer",
        L"orientation_sensor", L"gamepad", L"vr", L"ar"
    };
    for (const auto& f : powerful_features) {
        if (f == feature_name) return true;
    }
    return false;
}

const std::vector<std::wstring>& get_powerful_features() {
    static const std::vector<std::wstring> features = {
        L"geolocation", L"notifications", L"midi", L"camera",
        L"microphone", L"clipboard_read", L"clipboard_write",
        L"payment", L"credentials", L"usb", L"bluetooth", L"nfc"
    };
    return features;
}

} // namespace secure_features

} // namespace hre::security
