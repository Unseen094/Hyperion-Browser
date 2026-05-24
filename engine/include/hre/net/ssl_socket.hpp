#pragma once

#include <winsock2.h>
#include <windows.h>
#define SECURITY_WIN32
#include <schannel.h>
#include <sspi.h>
#include <string>
#include <mutex>

namespace hre::net {

class ssl_socket {
public:
    ssl_socket();
    ~ssl_socket();

    // Connect to a host using TLS (SChannel).
    bool connect(const std::string& host, const std::string& port);

    // Send encrypted data.
    int send(const std::string& data);

    // Receive and decrypt data.
    int recv(char* buffer, int len);

    // Close the connection.
    void close();

private:
    SOCKET m_sock;
    CredHandle m_cred_handle;
    CtxtHandle m_ctx_handle;
    bool m_cred_initialized;
    bool m_ctx_initialized;

    // Internal helper for TCP send
    int send_data(const char* data, int len);

    // Perform the TLS handshake
    bool perform_handshake(const std::string& host);
};

} // namespace hre::net
