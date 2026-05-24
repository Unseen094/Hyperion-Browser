#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <ctime>
#include <mutex>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace hyperion::security::cookie {

// Cookie attributes
struct CookieAttributes {
    std::string domain;
    std::string path = "/";
    std::time_t expires = 0; // 0 = session cookie
    bool secure = false;
    bool http_only = false;
    bool same_site = false;
    std::string same_site_mode = "Strict";
};

// Encrypted cookie value
enum class CookieSameSite : uint8_t {
    STRICT,
    LAX,
    NONE,
};

class CookieManager {
public:
    CookieManager() {
        // Generate encryption key if not present
        if (encryption_key_.empty()) {
            generate_encryption_key();
        }
    }
    
    // Set a cookie
    void set_cookie(const std::string& name, const std::string& value,
                    const std::string& domain, const CookieAttributes& attrs = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Hash domain and path for key
        std::string key = domain + ":" + (attrs.path.empty() ? "/" : attrs.path) + ":" + name;
        
        // Create secure cookie data
        SecureCookie cookie;
        cookie.plain_value = value;
        cookie.attributes = attrs;
        
        // Encrypt the value
        cookie.encrypted_value = encrypt_data(value);
        cookie.mac = generate_mac(cookie.encrypted_value);
        cookie.created_time = std::time(nullptr);
        cookie.last_accessed = cookie.created_time;
        
        cookies_[key] = cookie;
    }
    
    // Get a cookie (decrypts if found)
    std::string get_cookie(const std::string& name, const std::string& domain,
                           const std::string& path = "/") {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = domain + ":" + path + ":" + name;
        
        auto it = cookies_.find(key);
        if (it == cookies_.end()) {
            return {};
        }
        
        // Validate cookie (MAC check)
        if (!validate_cookie(it->second)) {
            cookies_.erase(it); // Remove invalid cookie
            return {};
        }
        
        it->second.last_accessed = std::time(nullptr);
        return it->second.plain_value;
    }
    
    // Delete a cookie
    void delete_cookie(const std::string& name, const std::string& domain,
                      const std::string& path = "/") {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = domain + ":" + path + ":" + name;
        cookies_.erase(key);
    }
    
    // List all cookies for domain
    std::vector<std::pair<std::string, std::string>> list_cookies(const std::string& domain) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<std::string, std::string>> result;
        
        for (const auto& [key, cookie] : cookies_) {
            if (key.find(domain + ":") == 0) {
                result.emplace_back(
                    extract_name_from_key(key),
                    cookie.plain_value
                );
            }
        }
        
        return result;
    }
    
    // Check cookie security attributes
    bool is_cookie_secure(const std::string& name, const std::string& domain, const std::string& path = "/") {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = domain + ":" + path + ":" + name;
        auto it = cookies_.find(key);
        if (it != cookies_.end()) {
            return it->second.attributes.secure;
        }
        return false;
    }
    
    // Sanitize cookies by domain and path
    void remove_cookies_for_domain(const std::string& domain) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cookies_.begin();
        while (it != cookies_.end()) {
            if (it->first.find(domain + ":") == 0) {
                it = cookies_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Cleanup expired cookies
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::time_t now = std::time(nullptr);
        auto it = cookies_.begin();
        
        while (it != cookies_.end()) {
            if (it->second.attributes.expires > 0 && it->second.attributes.expires < now) {
                it = cookies_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Total cookies count
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cookies_.size();
    }

private:
    struct SecureCookie {
        std::string plain_value;
        std::string encrypted_value;
        std::string mac;
        CookieAttributes attributes;
        std::time_t created_time;
        std::time_t last_accessed;
    };
    
    std::string encrypt_data(const std::string& data) {
        if (encryption_key_.empty()) {
            return data; // Not encrypted if no key
        }
        
        int out_len = data.size() + EVP_MAX_BLOCK_LENGTH;
        std::vector<unsigned char> out(out_len);
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return {};
        
        if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, 
                             reinterpret_cast<const unsigned char*>(encryption_key_.data()),
                             nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        
        int len;
        if (EVP_EncryptUpdate(ctx, out.data(), &len,
                           reinterpret_cast<const unsigned char*>(data.data()), 
                           static_cast<int>(data.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        
        int cipher_len = len;
        if (EVP_EncryptFinal_ex(ctx, out.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        cipher_len += len;
        
        EVP_CIPHER_CTX_free(ctx);
        out.resize(cipher_len);
        return std::string(out.begin(), out.end());
    }
    
    std::string decrypt_data(const std::string& encrypted) {
        if (encryption_key_.empty()) {
            return encrypted; // Not encrypted if no key
        }
        
        int out_len = encrypted.size();
        std::vector<unsigned char> out(out_len);
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return {};
        
        if (EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, 
                             reinterpret_cast<const unsigned char*>(encryption_key_.data()),
                             nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        
        int len;
        if (EVP_DecryptUpdate(ctx, out.data(), &len,
                           reinterpret_cast<const unsigned char*>(encrypted.data()), 
                           static_cast<int>(encrypted.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        
        int plain_len = len;
        if (EVP_DecryptFinal_ex(ctx, out.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        plain_len += len;
        
        EVP_CIPHER_CTX_free(ctx);
        out.resize(plain_len);
        return std::string(out.begin(), out.end());
    }
    
    std::string generate_mac(const std::string& data) {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int md_len;
        
        HMAC(EVP_sha256(),
              reinterpret_cast<const unsigned char*>(encryption_key_.data()),
              static_cast<int>(encryption_key_.size()),
              reinterpret_cast<const unsigned char*>(data.data()),
              static_cast<int>(data.size()),
              md, &md_len);
        
        return std::string(md, md + md_len);
    }
    
    bool validate_cookie(const SecureCookie& cookie) {
        if (encryption_key_.empty()) {
            return true; // Not encrypted if no key
        }
        
        std::string calculated_mac = generate_mac(cookie.encrypted_value);
        return cookie.mac == calculated_mac;
    }
    
    void generate_encryption_key() {
        encryption_key_.resize(16); // 128-bit key
        if (RAND_bytes(reinterpret_cast<unsigned char*>(encryption_key_.data()), 16) != 1) {
            // Fallback to random if hardware RNG not available
            for (size_t i = 0; i < 16; ++i) {
                encryption_key_[i] = static_cast<char>(rand() % 256);
            }
        }
    }
    
    std::string extract_name_from_key(const std::string& key) {
        size_t last_colon = key.rfind(':');
        if (last_colon != std::string::npos) {
            return key.substr(last_colon + 1);
        }
        return key;
    }
    
    std::unordered_map<std::string, SecureCookie> cookies_;
    std::string encryption_key_;
    mutable std::mutex mutex_;
};

// Session storage using cookies
class SessionStorage {
public:
    explicit SessionStorage(CookieManager& cookie_manager) : cookie_manager_(cookie_manager) {}
    
    void set_session_data(const std::string& key, const std::string& value,
                         bool permanent = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        session_data_[key] = value;
        if (permanent) {
            cookie_manager_.set_cookie(key, value, "session.local", 
                                     CookieAttributes{
                                         .domain = "session.local",
                                         .expires = std::time(nullptr) + 86400 * 30 // 30 days
                                     });
        }
    }
    
    std::string get_session_data(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cookie_value = cookie_manager_.get_cookie(key, "session.local");
        
        if (!cookie_value.empty()) {
            // Use cookie data
            return cookie_value;
        }
        
        // Check in-memory session
        auto it = session_data_.find(key);
        if (it != session_data_.end()) {
            return it->second;
        }
        
        return {};
    }
    
    void clear_session() {
        std::lock_guard<std::mutex> lock(mutex_);
        session_data_.clear();
        cookie_manager_.remove_cookies_for_domain("session.local");
    }

private:
    CookieManager& cookie_manager_;
    std::unordered_map<std::string, std::string> session_data_;
    mutable std::mutex mutex_;
};

} // namespace hyperion::security::cookie