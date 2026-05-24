#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace hre::media {

enum class key_system_type {
    UNKNOWN,
    WIDEVINE,
    PLAYREADY,
    CLEAR_KEY
};

struct key_system_config {
    key_system_type type = key_system_type::UNKNOWN;
    std::wstring init_data_type;
    std::vector<uint8_t> init_data;
    std::wstring session_id;
    std::wstring license_url;
    std::vector<uint8_t> server_certificate;
    std::vector<std::wstring> supported_capabilities;
    bool persistent_state = false;
    bool distinctive_identifier = false;
};

struct media_key_status {
    enum class status_type {
        USABLE,
        EXPIRED,
        OUTPUT_DOWNSCALED,
        OUTPUT_NOT_ALLOWED,
        STATUS_PENDING,
        INTERNAL_ERROR,
        RELEASED
    } status = status_type::INTERNAL_ERROR;

    std::wstring key_id;
    uint32_t system_code = 0;
};

struct media_key_session {
    std::wstring session_id;
    key_system_type key_system = key_system_type::UNKNOWN;
    std::vector<media_key_status> key_statuses;
    uint64_t expiration = 0;
    bool closed = false;

    using message_callback = std::function<void(const std::vector<uint8_t>& message, const std::wstring& destination_url)>;
    using key_status_callback = std::function<void(const std::vector<media_key_status>& statuses)>;

    message_callback on_message;
    key_status_callback on_key_status_change;
};

class eme_engine {
public:
    eme_engine();
    ~eme_engine();

    bool is_key_system_supported(const std::wstring& key_system) const;
    std::vector<std::wstring> get_supported_key_systems() const;

    std::shared_ptr<media_key_session> create_session(
        key_system_type system,
        const std::wstring& session_type,
        const std::wstring& init_data_type,
        const std::vector<uint8_t>& init_data,
        const std::vector<uint8_t>& server_certificate = {});

    void close_session(const std::wstring& session_id);
    void remove_session(const std::wstring& session_id);

    void update_session(const std::wstring& session_id, const std::vector<uint8_t>& response);

    void set_server_certificate(const std::vector<uint8_t>& cert);
    std::vector<uint8_t> get_server_certificate() const { return m_server_cert; }

private:
    std::shared_ptr<media_key_session> create_widevine_session(const key_system_config& config);
    std::shared_ptr<media_key_session> create_playready_session(const key_system_config& config);

    std::vector<uint8_t> m_server_cert;
    std::vector<std::shared_ptr<media_key_session>> m_sessions;
    uint32_t m_next_session_id = 1;
};

// ---- Widevine CDM Interface -----------------------------------------------

class widevine_cdm {
public:
    widevine_cdm();
    ~widevine_cdm();

    bool initialize(const std::vector<uint8_t>& server_certificate = {});
    bool generate_request(const std::string& init_data_type,
                          const std::vector<uint8_t>& init_data,
                          std::vector<uint8_t>& challenge,
                          std::wstring& destination_url);

    bool update_response(const std::vector<uint8_t>& response);
    std::vector<uint8_t> get_key_ids() const;
    bool decrypt(const uint8_t* encrypted_data, size_t data_size,
                 const uint8_t* key_id, size_t key_id_size,
                 const uint8_t* iv, size_t iv_size,
                 uint8_t* decrypted_data);

private:
    std::vector<uint8_t> m_session_key;
    std::vector<uint8_t> m_current_challenge;
    std::wstring m_current_url;
};

// ---- PlayReady Integration ------------------------------------------------

class playready_cdm {
public:
    playready_cdm();
    ~playready_cdm();

    bool initialize();
    bool generate_la_challenge(const std::vector<uint8_t>& init_data,
                               std::vector<uint8_t>& challenge);

    bool update_la_response(const std::vector<uint8_t>& response);
    bool decrypt(const uint8_t* encrypted_data, size_t data_size,
                 const uint8_t* key_id, size_t key_id_size,
                 uint8_t* decrypted_data);

private:
    std::vector<uint8_t> m_content_key;
};

// ---- Content Decryption Module (CDM) Factory ------------------------------

class cdm_factory {
public:
    static std::shared_ptr<eme_engine> create_eme_engine();
    static bool is_widevine_available();
    static bool is_playready_available();
    static bool is_clearkey_available();

    static std::wstring widevine_uuid();
    static std::wstring playready_uuid();
    static std::wstring clearkey_uuid();
};

} // namespace hre::media
