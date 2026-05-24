#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

namespace hre::security {

enum class secure_context_type {
    CRYPTO,       // Secure transport (HTTPS, WSS, etc.)
    LOCALHOST,    // Localhost or loopback
    FILE,         // File:// origins
    EXTENSION,    // Browser extension
    PRIVILEGED,   // Privileged contexts (about:blank, etc.)
    INSECURE      // Not a secure context
};

// ---- Secure Context Manager ----------------------------------------------

class secure_context_manager {
public:
    secure_context_manager();
    ~secure_context_manager();

    // Check if a URL is in a secure context
    bool is_secure_context(const std::wstring& url) const;
    bool is_potentially_trustworthy(const std::wstring& url) const;
    secure_context_type classify_context(const std::wstring& url) const;

    // Secure context restrictions
    bool is_feature_allowed_in_context(const std::wstring& url, const std::wstring& feature_name) const;
    bool is_powerful_feature_allowed(const std::wstring& url) const;

    // HTTPS-only / Upgrades
    bool is_upgrade_required(const std::wstring& url) const;
    std::wstring upgrade_to_https(const std::wstring& url) const;

    // Mixed content handling
    enum class mixed_content_type {
        BLOCKABLE,
        OPTIONALLY_BLOCKABLE,
        UPGRADABLE,
        NONE
    };

    mixed_content_type classify_mixed_content(const std::wstring& request_url) const;
    bool should_block_mixed_content(const std::wstring& request_url, mixed_content_type type) const;
    bool should_upgrade_mixed_content(const std::wstring& request_url) const;

    // Configuration
    void set_https_only_mode_enabled(bool enabled) { m_https_only = enabled; }
    bool is_https_only_mode_enabled() const { return m_https_only; }

    void set_block_mixed_content(bool block) { m_block_mixed = block; }
    bool is_mixed_content_blocked() const { return m_block_mixed; }

    void set_automatic_https_upgrades(bool upgrade) { m_auto_upgrade = upgrade; }
    bool is_automatic_https_upgrade_enabled() const { return m_auto_upgrade; }

    void set_localhost_is_secure(bool secure) { m_localhost_secure = secure; }
    bool is_localhost_considered_secure() const { return m_localhost_secure; }

    // Allow insecure origins (for testing/enterprise)
    void add_insecure_origin_allowed(const std::wstring& origin);
    bool is_insecure_origin_allowed(const std::wstring& origin) const;

    using mixed_content_callback = std::function<void(const std::wstring& url, mixed_content_type type)>;
    void set_mixed_content_callback(mixed_content_callback cb) { m_mixed_cb = cb; }

    // For UI indicator
    enum security_level { SECURE, WARNING, INSECURE, DANGEROUS };
    security_level get_security_level(const std::wstring& url) const;
    std::wstring get_security_icon(const std::wstring& url) const;

private:
    bool m_https_only = false;
    bool m_block_mixed = true;
    bool m_auto_upgrade = true;
    bool m_localhost_secure = true;

    std::vector<std::wstring> m_allowed_insecure_origins;
    mixed_content_callback m_mixed_cb;

    bool is_localhost(const std::wstring& host) const;
    bool is_loopback(const std::wstring& host) const;
};

// ---- Restricted Feature List ----------------------------------------------

namespace secure_features {
    bool requires_secure_context(const std::wstring& feature_name);
    const std::vector<std::wstring>& get_powerful_features();
}

} // namespace hre::security
