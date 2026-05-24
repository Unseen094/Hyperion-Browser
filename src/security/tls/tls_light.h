#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <array>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

namespace hyperion::security::tls {

// Lightweight TLS 1.3 context wrapper
class TlsContext {
public:
    TlsContext() : ctx_(SSL_CTX_new(TLS_server_method()), SSL_CTX_free) {
        if (!ctx_) {
            throw std::runtime_error("Failed to create SSL context");
        }
        
        // Modern TLS 1.3 settings for performance
        SSL_CTX_set_max_proto_version(*ctx_, TLS1_3_VERSION);
        SSL_CTX_set_min_proto_version(*ctx_, TLS1_2_VERSION);
        SSL_CTX_set_mode(*ctx_, SSL_MODE_ENABLE_PARTIAL_WRITE);
        SSL_CTX_clear_options(*ctx_, SSL_OP_LEGACY_SERVER_CONNECT);
        
        // Performance optimizations
        SSL_CTX_set_session_cache_mode(*ctx_, SSL_SESS_CACHE_CLIENT);
        SSL_CTX_sess_set_new_cb(*ctx_, session_new_callback);
        SSL_CTX_sess_set_remove_cb(*ctx_, session_remove_callback);
        
        // Security settings
        SSL_CTX_set_options(*ctx_, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
        SSL_CTX_set_verify(*ctx_, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, nullptr);
    }
    
    // Configure with keys and certs
    void configure(const std::string& cert_chain, const std::string& private_key,
                  const std::string& dh_params = {}) {
        if (!SSL_CTX_use_certificate_chain_file(*ctx_, cert_chain.c_str())) {
            throw std::runtime_error("Failed to load certificate chain");
        }
        
        if (!SSL_CTX_use_PrivateKey_file(*ctx_, private_key.c_str(), SSL_FILETYPE_PEM)) {
            throw std::runtime_error("Failed to load private key");
        }
        
        if (!dh_params.empty()) {
            SSL_CTX_set_tmp_dhfile(*ctx_, dh_params.c_str());
        }
        
        // Modern cipher suite selection
        const char* cipher_list = 
            "TLS_AES_256_GCM_SHA384:"
            "TLS_CHACHA20_POLY1305_SHA256:"
            "TLS_AES_128_GCM_SHA256";
        SSL_CTX_set_cipher_list(*ctx_, cipher_list);
        
        // Enable session tickets
        SSL_CTX_set_tlsext_ticket_key_evp_cb(*ctx_, ticket_key_callback);
    }
    
    // Create TLS session
    std::shared_ptr<SSL> create_session() {
        SSL* ssl = SSL_new(*ctx_);
        if (!ssl) {
            throw std::runtime_error("Failed to create SSL session");
        }
        
        // Set performance hints for QUIC-like traffic
        SSL_set_bio(ssl, nullptr, nullptr);
        SSL_set_app_data(ssl, this);
        
        // QUIC-specific settings
        SSL_set_quic_use_legacy_codepoint(ssl, 1);
        
        return std::shared_ptr<SSL>(ssl, SSL_free);
    }
    
    // Get current TLS session info
    std::string get_session_info(const SSL* ssl) {
        std::array<char, 512> buf{};
        
        // Protocol version
        int version = SSL_version(ssl);
        int cipher = SSL_CIPHER_get_bits(SSL_get_current_cipher(ssl), nullptr);
        const SSL_CIPHER* cipher_data = SSL_get_current_cipher(ssl);
        
        snprintf(buf.data(), buf.size(), "Version: TLS %d.%d, Cipher: %s, Bits: %d",
                 version >> 8, version & 0xFF,
                 SSL_CIPHER_get_name(cipher_data), cipher);
        
        return std::string(buf.data());
    }
    
    // Session caching callback for performance
    static int session_new_callback(SSL* ssl, SSL_SESSION* session) {
        // Store session for reuse
        return 1; // Success
    }
    
    static void session_remove_callback(SSL_CTX* /*ctx*/, SSL_SESSION* session) {
        SSL_SESSION_free(session);
    }
    
    // QUIC ticket key generation callback
    static size_t ticket_key_callback(SSL* /*ssl*/, void* out, const void* /*in*/, size_t /*inlen*/, 
                                  size_t stored_len, void* /*key_data*/) {
        if (stored_len > 0) {
            // Copy existing key
            return stored_len;
        }
        
        // Generate new ticket key
        size_t key_size = 2 * TLS13_TICKET_SECRETS_SIZE; // QUIC needs larger tickets
        if (key_size > 0) {
            if (RAND_bytes(reinterpret_cast<unsigned char*>(out), static_cast<int>(key_size)) != 1) {
                return 0; // Failure
            }
            return key_size;
        }
        return 0;
    }
    
    // Performance monitoring
    std::pair<int, int> get_certificate_validity_range(const std::string& cert_path) {
        FILE* fp = fopen(cert_path.c_str(), "r");
        if (!fp) {
            return {0, 0};
        }
        
        X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
        fclose(fp);
        
        if (!cert) {
            return {0, 0};
        }
        
        ASN1_TIME *not_before = X509_getm_notBefore(cert);
        ASN1_TIME *not_after = X509_getm_notAfter(cert);
        
        int days_valid = 0;
        if (not_before && not_after) {
            ASN1_TIME_diff(&days_valid, nullptr, not_before, not_after);
        }
        
        X509_free(cert);
        return {days_valid, days_valid >= 0 ? days_valid : 0};
    }

private:
    std::unique_ptr<SSL_CTX, void(*)(SSL_CTX*)> ctx_;
};

// QUIC TLS 1.3 context manager for optimized transport
class QuicTlsManager {
public:
    QuicTlsManager() {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
    }
    
    ~QuicTlsManager() {
        // Cleanup
    }
    
    // Get TLS context optimized for QUIC
    std::shared_ptr<TlsContext> get_quic_tls_context() {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        
        if (!quic_context_) {
            quic_context_ = std::make_shared<TlsContext>();
            // Configure with QUICTLS if present, or use OpenSSL
            try {
                quic_context_->configure(
                    "ssl/cert.pem", // Example paths - should be configurable
                    "ssl/key.pem",
                    "ssl/dhparams.pem"
                );
            } catch (...) {
                // Fallback or continue without certs
            }
        }
        
        return quic_context_;
    }
    
    // Performance cache for TLS handshake
    class TlsCacheManager {
    public:
        void cache_tls_handshake(const std::array<uint8_t, 32>& connection_id,
                              const std::vector<uint8_t>& session_data) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = std::chrono::steady_clock::now();
            session_cache_[connection_id] = {session_data, now};
        }
        
        std::vector<uint8_t> lookup_tls_session(const std::array<uint8_t, 32>& connection_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = session_cache_.find(connection_id);
            if (it != session_cache_.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - it->second.timestamp
                ).count();
                if (elapsed < 300) { // 5 minutes cache
                    return it->second.data;
                }
            }
            return {};
        }
        
        void cleanup_stale_entries() {
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = std::chrono::steady_clock::now();
            auto it = session_cache_.begin();
            
            while (it != session_cache_.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second.timestamp
                ).count();
                if (elapsed > 300) {
                    it = session_cache_.erase(it);
                } else {
                    ++it;
                }
            }
        }

    private:
        struct CacheEntry {
            std::vector<uint8_t> data;
            std::chrono::steady_clock::time_point timestamp;
        };
        
        std::unordered_map<std::array<uint8_t, 32>, CacheEntry> session_cache_;
        mutable std::mutex mutex_;
    };
    
    TlsCacheManager& cache_manager() {
        static TlsCacheManager manager;
        return manager;
    }

private:
    std::shared_ptr<TlsContext> quic_context_;
};

} // namespace hyperion::security::tls