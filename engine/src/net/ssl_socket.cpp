#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "hre/net/ssl_socket.hpp"
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <codecvt>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#include <locale>

#pragma comment(lib, "secur32.lib")

namespace hre::net {

ssl_socket::ssl_socket() : m_sock(INVALID_SOCKET), m_cred_handle{}, m_ctx_handle{}, m_cred_initialized(false), m_ctx_initialized(false) {
    SecInvalidateHandle(&m_cred_handle);
    SecInvalidateHandle(&m_ctx_handle);
}

ssl_socket::~ssl_socket() {
    close();
    if (m_ctx_initialized) {
        DeleteSecurityContext(&m_ctx_handle);
        m_ctx_initialized = false;
    }
    if (m_cred_initialized) {
        FreeCredentialsHandle(&m_cred_handle);
        m_cred_initialized = false;
    }
}

bool ssl_socket::connect(const std::string& host, const std::string& port) {
    struct addrinfo hints{}, *info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &info) != 0) {
        return false;
    }

    m_sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (m_sock == INVALID_SOCKET) {
        freeaddrinfo(info);
        return false;
    }

    if (::connect(m_sock, info->ai_addr, static_cast<int>(info->ai_addrlen)) == SOCKET_ERROR) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        freeaddrinfo(info);
        return false;
    }
    freeaddrinfo(info);

    SCHANNEL_CRED schannel_cred = {0};
    schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
    schannel_cred.grbitEnabledProtocols = SP_PROT_TLS1_2 | SP_PROT_TLS1_3;
    schannel_cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION;

    SECURITY_STATUS status = AcquireCredentialsHandleW(
        nullptr,
        const_cast<LPWSTR>(UNISP_NAME_W),
        SECPKG_CRED_OUTBOUND,
        nullptr,
        &schannel_cred,
        nullptr,
        nullptr,
        &m_cred_handle,
        nullptr
    );

    if (status != SEC_E_OK) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    m_cred_initialized = true;

    return perform_handshake(host);
}

bool ssl_socket::perform_handshake(const std::string& host) {
    DWORD context_attrs = 0;
    SecBufferDesc out_buf_desc = {0};
    SecBuffer out_sec_buf = {0};
    SecBufferDesc in_buf_desc = {0};
    SecBuffer in_sec_buf = {0};
    SECURITY_STATUS status;

    out_sec_buf.cbBuffer = 0;
    out_sec_buf.BufferType = SECBUFFER_TOKEN;
    out_sec_buf.pvBuffer = nullptr;

    out_buf_desc.cBuffers = 1;
    out_buf_desc.pBuffers = &out_sec_buf;
    out_buf_desc.ulVersion = SECBUFFER_VERSION;

    std::wstring whost(host.begin(), host.end());

    status = InitializeSecurityContextW(
        &m_cred_handle,
        nullptr,
        const_cast<SEC_WCHAR*>(whost.c_str()),
        ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM,
        0,
        SECURITY_NATIVE_DREP,
        nullptr,
        0,
        &m_ctx_handle,
        &out_buf_desc,
        &context_attrs,
        nullptr
    );

    if (status != SEC_I_CONTINUE_NEEDED) {
        return false;
    }

    if (out_sec_buf.cbBuffer > 0) {
        send_data(static_cast<const char*>(out_sec_buf.pvBuffer), out_sec_buf.cbBuffer);
        FreeContextBuffer(out_sec_buf.pvBuffer);
        out_sec_buf.pvBuffer = nullptr;
    }
    m_ctx_initialized = true;

    while (true) {
        char recv_buf[4096];
        int bytes_received = ::recv(m_sock, recv_buf, sizeof(recv_buf), 0);
        if (bytes_received <= 0) return false;

        in_sec_buf.cbBuffer = bytes_received;
        in_sec_buf.BufferType = SECBUFFER_TOKEN;
        in_sec_buf.pvBuffer = recv_buf;

        in_buf_desc.cBuffers = 1;
        in_buf_desc.pBuffers = &in_sec_buf;
        in_buf_desc.ulVersion = SECBUFFER_VERSION;

        out_sec_buf.cbBuffer = 0;
        out_sec_buf.pvBuffer = nullptr;
        out_sec_buf.BufferType = SECBUFFER_TOKEN;

        out_buf_desc.cBuffers = 1;
        out_buf_desc.pBuffers = &out_sec_buf;
        out_buf_desc.ulVersion = SECBUFFER_VERSION;

        status = InitializeSecurityContextW(
            &m_cred_handle,
            &m_ctx_handle,
            nullptr,
            ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM,
            0,
            SECURITY_NATIVE_DREP,
            &in_buf_desc,
            0,
            &m_ctx_handle,
            &out_buf_desc,
            &context_attrs,
            nullptr
        );

        if (out_sec_buf.cbBuffer > 0) {
            send_data(static_cast<const char*>(out_sec_buf.pvBuffer), out_sec_buf.cbBuffer);
            FreeContextBuffer(out_sec_buf.pvBuffer);
        }

        if (status == SEC_E_OK) {
            PCCERT_CONTEXT cert_ctx = nullptr;
            SECURITY_STATUS cert_status = QueryContextAttributesW(&m_ctx_handle, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &cert_ctx);
            if (cert_status != SEC_E_OK || !cert_ctx) {
                return false;
            }
            // Build certificate chain
            PCCERT_CHAIN_CONTEXT chain_ctx = nullptr;
            CERT_CHAIN_PARA chain_para = {};
            chain_para.cbSize = sizeof(CERT_CHAIN_PARA);
            if (!CertGetCertificateChain(nullptr, cert_ctx, nullptr, nullptr, &chain_para, 0, nullptr, &chain_ctx)) {
                CertFreeCertificateContext(cert_ctx);
                return false;
            }
            // Verify the chain
            CERT_CHAIN_POLICY_STATUS policy_status = {};
            CERT_CHAIN_POLICY_PARA policy_para = {};
            policy_para.cbSize = sizeof(policy_para);
            BOOL verify = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain_ctx, &policy_para, &policy_status);
            CertFreeCertificateChain(chain_ctx);
            CertFreeCertificateContext(cert_ctx);
            if (!verify || policy_status.dwError != 0) {
                return false;
            }
            return true;
        } else if (status == SEC_I_CONTINUE_NEEDED) {
            continue;
        } else {
            return false;
        }
    }
}

int ssl_socket::send_data(const char* data, int len) {
    if (m_sock == INVALID_SOCKET) return SOCKET_ERROR;
    return ::send(m_sock, data, len, 0);
}

int ssl_socket::send(const std::string& data) {
    if (m_sock == INVALID_SOCKET || !m_ctx_initialized) return SOCKET_ERROR;

    SecPkgContext_StreamSizes sizes;
    SECURITY_STATUS status = QueryContextAttributesW(&m_ctx_handle, SECPKG_ATTR_STREAM_SIZES, &sizes);
    if (status != SEC_E_OK) return SOCKET_ERROR;

    std::vector<BYTE> buffer(sizes.cbHeader + data.size() + sizes.cbTrailer);
    memcpy(buffer.data() + sizes.cbHeader, data.c_str(), data.size());

    SecBufferDesc buf_desc;
    SecBuffer sec_buf;
    sec_buf.cbBuffer = static_cast<ULONG>(sizes.cbHeader + data.size() + sizes.cbTrailer);
    sec_buf.BufferType = SECBUFFER_TOKEN;
    sec_buf.pvBuffer = buffer.data();

    buf_desc.cBuffers = 1;
    buf_desc.pBuffers = &sec_buf;
    buf_desc.ulVersion = SECBUFFER_VERSION;

    status = EncryptMessage(&m_ctx_handle, 0, &buf_desc, 0);
    if (status != SEC_E_OK) return SOCKET_ERROR;

    return send_data(reinterpret_cast<const char*>(buffer.data()), sec_buf.cbBuffer);
}

int ssl_socket::recv(char* buffer, int len) {
    if (m_sock == INVALID_SOCKET || !m_ctx_initialized) return SOCKET_ERROR;

    char recv_buf[4096];
    int bytes_read = ::recv(m_sock, recv_buf, sizeof(recv_buf), 0);
    if (bytes_read <= 0) return bytes_read;

    SecBufferDesc buf_desc;
    SecBuffer sec_buf;
    sec_buf.cbBuffer = bytes_read;
    sec_buf.BufferType = SECBUFFER_TOKEN;
    sec_buf.pvBuffer = recv_buf;

    buf_desc.cBuffers = 1;
    buf_desc.pBuffers = &sec_buf;
    buf_desc.ulVersion = SECBUFFER_VERSION;

    SECURITY_STATUS status = DecryptMessage(&m_ctx_handle, &buf_desc, 0, nullptr);
    if (status != SEC_E_OK) return -1;

    int decrypted_len = sec_buf.cbBuffer;
    if (decrypted_len > len) decrypted_len = len;
    memcpy(buffer, recv_buf, decrypted_len);

    return decrypted_len;
}

void ssl_socket::close() {
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
}

} // namespace hre::net