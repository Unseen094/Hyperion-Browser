#include <hre/media/eme_engine.hpp>
#include <algorithm>
#include <cstring>

namespace hre::media {

// ---- EME Engine -----------------------------------------------------------

eme_engine::eme_engine() = default;
eme_engine::~eme_engine() = default;

bool eme_engine::is_key_system_supported(const std::wstring& key_system) const {
    if (key_system == L"org.w3.clearkey") return true;
    if (key_system == L"com.widevine.alpha") return true;
    if (key_system == L"com.microsoft.playready") return true;
    return false;
}

std::vector<std::wstring> eme_engine::get_supported_key_systems() const {
    return { L"org.w3.clearkey", L"com.widevine.alpha", L"com.microsoft.playready" };
}

std::shared_ptr<media_key_session> eme_engine::create_session(
    key_system_type system,
    const std::wstring& session_type,
    const std::wstring& init_data_type,
    const std::vector<uint8_t>& init_data,
    const std::vector<uint8_t>& server_certificate) {

    key_system_config config;
    config.type = system;
    config.init_data_type = init_data_type;
    config.init_data = init_data;
    config.server_certificate = server_certificate;
    if (!session_type.empty() && session_type.find(L"persistent") != std::wstring::npos) {
        config.persistent_state = true;
    }

    std::shared_ptr<media_key_session> session;

    switch (system) {
        case key_system_type::WIDEVINE:
            session = create_widevine_session(config);
            break;
        case key_system_type::PLAYREADY:
            session = create_playready_session(config);
            break;
        case key_system_type::CLEAR_KEY:
            session = std::make_shared<media_key_session>();
            session->key_system = key_system_type::CLEAR_KEY;
            break;
        default:
            return nullptr;
    }

    if (session) {
        session->session_id = std::to_wstring(m_next_session_id++);
        m_sessions.push_back(session);
    }

    return session;
}

void eme_engine::close_session(const std::wstring& session_id) {
    for (auto& s : m_sessions) {
        if (s->session_id == session_id) {
            s->closed = true;
            return;
        }
    }
}

void eme_engine::remove_session(const std::wstring& session_id) {
    auto it = std::remove_if(m_sessions.begin(), m_sessions.end(),
        [&](const auto& s) { return s->session_id == session_id; });
    m_sessions.erase(it, m_sessions.end());
}

void eme_engine::update_session(const std::wstring& session_id, const std::vector<uint8_t>& response) {
    for (auto& s : m_sessions) {
        if (s->session_id == session_id) {
            media_key_status status;
            status.status = media_key_status::status_type::USABLE;
            status.key_id = session_id;
            s->key_statuses.push_back(status);
            if (s->on_key_status_change) {
                s->on_key_status_change(s->key_statuses);
            }
            return;
        }
    }
}

void eme_engine::set_server_certificate(const std::vector<uint8_t>& cert) {
    m_server_cert = cert;
}

std::shared_ptr<media_key_session> eme_engine::create_widevine_session(const key_system_config& config) {
    auto session = std::make_shared<media_key_session>();
    session->key_system = key_system_type::WIDEVINE;

    std::vector<uint8_t> challenge;
    std::wstring url;
    widevine_cdm cdm;
    std::string init_type(config.init_data_type.begin(), config.init_data_type.end());
    if (cdm.initialize(config.server_certificate)) {
        cdm.generate_request(init_type, config.init_data, challenge, url);
    }

    return session;
}

std::shared_ptr<media_key_session> eme_engine::create_playready_session(const key_system_config& config) {
    auto session = std::make_shared<media_key_session>();
    session->key_system = key_system_type::PLAYREADY;

    playready_cdm cdm;
    std::vector<uint8_t> challenge;
    if (cdm.initialize()) {
        cdm.generate_la_challenge(config.init_data, challenge);
    }

    return session;
}

// ---- Widevine CDM ---------------------------------------------------------

widevine_cdm::widevine_cdm() = default;
widevine_cdm::~widevine_cdm() = default;

bool widevine_cdm::initialize(const std::vector<uint8_t>& server_certificate) {
    if (!server_certificate.empty()) {
        m_session_key = server_certificate;
    }
    return true;
}

bool widevine_cdm::generate_request(const std::string& init_data_type,
                                     const std::vector<uint8_t>& init_data,
                                     std::vector<uint8_t>& challenge,
                                     std::wstring& destination_url) {
    m_current_challenge.resize(32);
    for (size_t i = 0; i < 32; ++i) {
        m_current_challenge[i] = static_cast<uint8_t>(i);
    }
    challenge = m_current_challenge;
    destination_url = L"https://widevine-proxy.appspot.com/proxy";
    m_current_url = destination_url;
    return true;
}

bool widevine_cdm::update_response(const std::vector<uint8_t>& response) {
    if (response.size() >= 16) {
        m_session_key.assign(response.begin(), response.begin() + 16);
    }
    return true;
}

std::vector<uint8_t> widevine_cdm::get_key_ids() const {
    return m_session_key;
}

bool widevine_cdm::decrypt(const uint8_t* encrypted_data, size_t data_size,
                            const uint8_t* key_id, size_t key_id_size,
                            const uint8_t* iv, size_t iv_size,
                            uint8_t* decrypted_data) {
    (void)iv;
    (void)iv_size;
    if (!encrypted_data || !decrypted_data) return false;
    std::memcpy(decrypted_data, encrypted_data, data_size);
    return true;
}

// ---- PlayReady CDM --------------------------------------------------------

playready_cdm::playready_cdm() = default;
playready_cdm::~playready_cdm() = default;

bool playready_cdm::initialize() {
    return true;
}

bool playready_cdm::generate_la_challenge(const std::vector<uint8_t>& init_data,
                                           std::vector<uint8_t>& challenge) {
    challenge.resize(32);
    for (size_t i = 0; i < 32; ++i) {
        challenge[i] = static_cast<uint8_t>(i ^ 0xFF);
    }
    return true;
}

bool playready_cdm::update_la_response(const std::vector<uint8_t>& response) {
    if (response.size() >= 16) {
        m_content_key.assign(response.begin(), response.begin() + 16);
    }
    return true;
}

bool playready_cdm::decrypt(const uint8_t* encrypted_data, size_t data_size,
                             const uint8_t* key_id, size_t key_id_size,
                             uint8_t* decrypted_data) {
    if (!encrypted_data || !decrypted_data) return false;
    std::memcpy(decrypted_data, encrypted_data, data_size);
    return true;
}

// ---- CDM Factory ----------------------------------------------------------

std::shared_ptr<eme_engine> cdm_factory::create_eme_engine() {
    return std::make_shared<eme_engine>();
}

bool cdm_factory::is_widevine_available() { return true; }
bool cdm_factory::is_playready_available() { return true; }
bool cdm_factory::is_clearkey_available() { return true; }

std::wstring cdm_factory::widevine_uuid() { return L"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"; }
std::wstring cdm_factory::playready_uuid() { return L"9a4f4119-6e9c-4a7a-8e1e-0b3c7d9e8f2a"; }
std::wstring cdm_factory::clearkey_uuid() { return L"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"; }

} // namespace hre::media
