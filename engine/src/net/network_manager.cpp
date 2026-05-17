#include <hre/net/network_manager.hpp>
#include <hyperion/platform/logging.hpp>
#include <winsock2.h>
#include <ws2tcpip.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>
#include <thread>
#include <sstream>
#include <iostream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

namespace hre::net {

struct http_response {
    int status_code;
    std::map<std::string, std::string> headers;
    std::string body;
};

network_manager::network_manager() {
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

network_manager::~network_manager() {
    WSACleanup();
}

void network_manager::fetch_async(const std::wstring& url, std::function<void(const std::string&)> callback) {
    std::thread([this, url, callback]() {
        std::string result = perform_request(url);
        callback(result);
    }).detach();
}

http_response parse_raw_response(const std::string& raw) {
    http_response res;
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return res;

    std::string header_part = raw.substr(0, header_end);
    std::string body_part = raw.substr(header_end + 4);

    std::stringstream ss(header_part);
    std::string line;
    
    // Parse Status Line: HTTP/1.1 200 OK
    if (std::getline(ss, line)) {
        size_t first_space = line.find(" ");
        size_t second_space = line.find(" ", first_space + 1);
        if (first_space != std::string::npos && second_space != std::string::npos) {
            res.status_code = std::stoi(line.substr(first_space + 1, second_space - first_space - 1));
        }
    }

    // Parse Headers
    while (std::getline(ss, line) && line != "\r") {
        size_t colon = line.find(":");
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" "));
            if (!value.empty() && value.back() == '\r') value.pop_back();
            res.headers[key] = value;
        }
    }

    // Handle Chunked Encoding
    if (res.headers.count("Transfer-Encoding") && res.headers["Transfer-Encoding"] == "chunked") {
        std::string decoded_body;
        size_t pos = 0;
        while (pos < body_part.size()) {
            size_t line_end = body_part.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            
            std::string size_hex = body_part.substr(pos, line_end - pos);
            long chunk_size = std::stol(size_hex, nullptr, 16);
            if (chunk_size == 0) break;

            decoded_body.append(body_part.substr(line_end + 2, chunk_size));
            pos = line_end + 2 + chunk_size + 2; // skip body and trailing \r\n
        }
        res.body = decoded_body;
    } else {
        res.body = body_part;
    }

    return res;
}

std::string network_manager::perform_request(const std::wstring& url_w) {
    std::string url(url_w.begin(), url_w.end());
    std::string host, path = "/";
    int port = 80;
    bool use_ssl = false;

    size_t protocol_end = url.find("://");
    if (protocol_end != std::string::npos) {
        std::string protocol = url.substr(0, protocol_end);
        use_ssl = (protocol == "https");
        if (use_ssl) port = 443;
        std::string host_part = url.substr(protocol_end + 3);
        size_t path_start = host_part.find("/");
        if (path_start != std::string::npos) {
            host = host_part.substr(0, path_start);
            path = host_part.substr(path_start);
        } else {
            host = host_part;
        }
    } else {
        // No protocol - treat entire URL as host
        size_t path_start = url.find("/");
        if (path_start != std::string::npos) {
            host = url.substr(0, path_start);
            path = url.substr(path_start);
        } else {
            host = url;
        }
    }

    if (host.empty()) {
        return "<html><body><h1>Invalid URL</h1></body></html>";
    }

    // Resolve DNS
    size_t path_start = host_part.find("/");
    if (path_start != std::string::npos) {
        host = host_part.substr(0, path_start);
        path = host_part.substr(path_start);
    } else {
        host = host_part;
    }

    // Resolve DNS
    addrinfo hints = { 0 }, *info;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &info) != 0) {
        return "<html><body><h1>DNS Error</h1></body></html>";
    }

    SOCKET sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (connect(sock, info->ai_addr, (int)info->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(info);
        closesocket(sock);
        return "<html><body><h1>Connection Failed</h1></body></html>";
    }
    freeaddrinfo(info);

    // [FUTURE: SChannel/TLS Handshake logic would be inserted here for HTTPS]
    // For this completion pass, we focus on the custom HTTP/1.1 logic.

    std::stringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "User-Agent: HyperionBrowser/0.1.0\r\n";
    request << "Accept: */*\r\n";
    request << "Accept-Encoding: identity\r\n"; // We don't support gzip yet
    request << "Connection: close\r\n\r\n";

    std::string req_str = request.str();
    send(sock, req_str.c_str(), (int)req_str.length(), 0);

    std::string response_raw;
    char buffer[8192];
    int bytes;
    while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response_raw.append(buffer, bytes);
    }
    closesocket(sock);

    http_response res = parse_raw_response(response_raw);
    
    // Redirect handling foundation
    if (res.status_code >= 300 && res.status_code < 400 && res.headers.count("Location")) {
        // In a real implementation, we'd recursively fetch the new location
        HYPERION_LOG_INFO("Redirecting to: %s", res.headers["Location"].c_str());
    }

    return res.body;
}

} // namespace hre::net
