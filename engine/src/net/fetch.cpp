#define NOMINMAX
#include "hre/net/fetch.hpp"
#include "hre/net/http2/client.hpp"
#include "hre/net/ssl_socket.hpp"
#include <algorithm>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>

namespace hre::net {

std::string fetch_response::text() const {
    return std::string(body.begin(), body.end());
}

std::map<std::string, std::string> fetch_response::json() const {
    std::map<std::string, std::string> result;
    // Simplified: parse basic JSON key-value pairs
    std::string text_body = this->text();
    if (text_body.empty()) return result;

    // Very basic JSON parsing
    size_t pos = text_body.find('{');
    if (pos == std::string::npos) return result;
    pos = text_body.find('"', pos);
    while (pos != std::string::npos) {
        size_t key_end = text_body.find('"', pos + 1);
        if (key_end == std::string::npos) break;
        std::string key = text_body.substr(pos + 1, key_end - pos - 1);

        size_t colon = text_body.find(':', key_end);
        if (colon == std::string::npos) break;

        size_t val_start = text_body.find_first_not_of(" \t\n\r", colon + 1);
        if (val_start == std::string::npos) break;

        std::string value;
        if (text_body[val_start] == '"') {
            size_t val_end = text_body.find('"', val_start + 1);
            if (val_end == std::string::npos) break;
            value = text_body.substr(val_start + 1, val_end - val_start - 1);
            pos = text_body.find('"', val_end + 1);
        } else {
            size_t val_end = text_body.find_first_of(",}", val_start);
            if (val_end == std::string::npos) break;
            value = text_body.substr(val_start, val_end - val_start);
            pos = text_body.find('"', val_end);
        }

        // Trim value
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
        result[key] = value;
    }

    return result;
}

fetch_request fetch_api::process_request(const fetch_request& req) {
    fetch_request processed = req;

    // Normalize URL
    if (processed.url.find("://") == std::string::npos) {
        processed.url = "https://" + processed.url;
    }

    // Set default headers
    if (processed.headers.find("User-Agent") == processed.headers.end()) {
        processed.headers["User-Agent"] = "Hyperion/1.0";
    }
    if (processed.headers.find("Accept") == processed.headers.end()) {
        processed.headers["Accept"] = "*/*";
    }

    // CORS pre-processing
    if (processed.cors == cors_mode::Cors || processed.cors == cors_mode::NoCors) {
        if (processed.credentials == credentials_mode::Include) {
            processed.headers["Origin"] = "*"; // Simplified
        }
    }

    return processed;
}

fetch_response fetch_api::process_response(const fetch_response& resp, const fetch_request& req) {
    fetch_response result = resp;

    // Determine response type based on CORS
    if (req.cors == cors_mode::NoCors) {
        result.type = "opaque";
    } else if (req.cors == cors_mode::Cors) {
        std::string acao;
        auto it = resp.headers.find("Access-Control-Allow-Origin");
        if (it != resp.headers.end()) {
            acao = it->second;
        }
        if (acao == "*" || acao == req.headers.at("Origin")) {
            result.type = "cors";
        } else {
            result.type = "opaque";
            return result;
        }
    } else {
        result.type = "basic";
    }

    result.ok = (result.status_code >= 200 && result.status_code < 300);
    return result;
}

std::string fetch_api::generate_id() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 15);
    std::string id = "req_";
    for (int i = 0; i < 16; ++i) {
        id += "0123456789abcdef"[dist(rng)];
    }
    return id;
}

void fetch_api::fetch(const fetch_request& req, std::function<void(const fetch_response&)> callback) {
    fetch_request processed = process_request(req);

    pending_request pr;
    pr.request = processed;
    pr.callback = callback;
    pr.id = generate_id();
    pending_.push_back(pr);

    do_fetch(static_cast<int>(pending_.size()) - 1);
}

void fetch_api::fetch_async(const fetch_request& req, std::function<void(const fetch_response&)> callback) {
    // For async, we'd spawn a thread or use the event loop
    // For now, just do synchronous fetch
    fetch(req, callback);
}

void fetch_api::abort(const std::string& request_id) {
    for (auto& pr : pending_) {
        if (pr.id == request_id) {
            pr.aborted = true;
            fetch_response resp;
            resp.status_code = 0;
            resp.ok = false;
            resp.status_text = "Aborted";
            if (pr.callback) pr.callback(resp);
            break;
        }
    }
}

void fetch_api::do_fetch(int index) {
    if (index < 0 || index >= (int)pending_.size()) return;
    auto& pr = pending_[index];

    if (pr.aborted) return;

    const auto& req = pr.request;
    fetch_response response;

    // Decide whether to use HTTP/2 or HTTP/1.1
    bool use_h2 = req.prefer_h2 && req.url.find("https://") == 0;

    if (use_h2) {
        http2::http2_client h2_client;
        h2_client.fetch(req.url, [&](const http2::http2_response& h2_resp) {
            response.status_code = h2_resp.status_code;
            response.headers = h2_resp.headers;
            response.body = h2_resp.body;
            response.ok = (h2_resp.status_code >= 200 && h2_resp.status_code < 300);
            response.url = req.url;

            for (auto& [k, v] : h2_resp.headers) {
                if (k == "Location") {
                    response.redirected = true;
                }
            }
        });
    } else {
        // HTTP/1.1 fallback (simplified)
        try {
            bool use_ssl = req.url.find("https://") == 0;
            std::string host;
            std::string path = "/";
            int port = use_ssl ? 443 : 80;

            // Parse URL
            size_t proto_end = req.url.find("://");
            size_t host_start = (proto_end != std::string::npos) ? proto_end + 3 : 0;
            size_t path_start = req.url.find('/', host_start);
            if (path_start != std::string::npos) {
                host = req.url.substr(host_start, path_start - host_start);
                path = req.url.substr(path_start);
            } else {
                host = req.url.substr(host_start);
            }

            size_t colon_pos = host.find(':');
            if (colon_pos != std::string::npos) {
                port = std::stoi(host.substr(colon_pos + 1));
                host = host.substr(0, colon_pos);
            }

            if (use_ssl) {
                ssl_socket sock;
                if (sock.connect(host, std::to_string(port))) {
                    // Build request
                    std::string request_str = req.method + " " + path + " HTTP/1.1\r\n";
                    request_str += "Host: " + host + "\r\n";
                    for (auto& [k, v] : req.headers) {
                        request_str += k + ": " + v + "\r\n";
                    }
                    request_str += "Connection: close\r\n";
                    if (!req.body.empty()) {
                        request_str += "Content-Length: " + std::to_string(req.body.size()) + "\r\n";
                    }
                    request_str += "\r\n";
                    if (!req.body.empty()) {
                        request_str += std::string(req.body.begin(), req.body.end());
                    }

                    sock.send(request_str);

                    // Read response
                    std::vector<uint8_t> resp_buf;
                    char buf[4096];
                    int n;
                    while ((n = sock.recv(buf, sizeof(buf))) > 0) {
                        resp_buf.insert(resp_buf.end(), buf, buf + n);
                    }
                    sock.close();

                    // Parse response
                    std::string resp_str(resp_buf.begin(), resp_buf.end());
                    size_t header_end = resp_str.find("\r\n\r\n");
                    if (header_end != std::string::npos) {
                        std::string header_section = resp_str.substr(0, header_end);
                        response.body.assign(resp_buf.begin() + header_end + 4, resp_buf.end());

                        // Parse status line
                        size_t first_line = header_section.find("\r\n");
                        if (first_line != std::string::npos) {
                            std::string status_line = header_section.substr(0, first_line);
                            size_t code_start = status_line.find(' ');
                            if (code_start != std::string::npos) {
                                ++code_start;
                                size_t code_end = status_line.find(' ', code_start);
                                if (code_end != std::string::npos) {
                                    response.status_code = std::stoi(status_line.substr(code_start, code_end - code_start));
                                    response.status_text = status_line.substr(code_end + 1);
                                }
                            }
                        }

                        // Parse headers
                        size_t line_start = first_line + 2;
                        while (line_start < header_section.size()) {
                            size_t line_end = header_section.find("\r\n", line_start);
                            if (line_end == std::string::npos) break;
                            std::string line = header_section.substr(line_start, line_end - line_start);
                            size_t colon = line.find(':');
                            if (colon != std::string::npos) {
                                std::string key = line.substr(0, colon);
                                std::string val = line.substr(colon + 2);
                                response.headers[key] = val;
                            }
                            line_start = line_end + 2;
                        }

                        response.ok = (response.status_code >= 200 && response.status_code < 300);
                        response.url = req.url;
                    }
                }
            }
        } catch (const std::exception& e) {
            response.status_code = 0;
            response.ok = false;
            response.status_text = e.what();
        }
    }

    if (!pr.aborted) {
        complete_fetch(index, response);
    }
}

void fetch_api::complete_fetch(int index, const fetch_response& response) {
    if (index < 0 || index >= (int)pending_.size()) return;
    auto& pr = pending_[index];
    fetch_response processed = process_response(response, pr.request);

    // Handle redirect
    if (processed.redirected && pr.request.redirect == redirect_mode::Follow) {
        auto loc_it = processed.headers.find("Location");
        if (loc_it != processed.headers.end()) {
            fetch_request redirect_req = pr.request;
            redirect_req.url = loc_it->second;
            pending_.push_back({redirect_req, pr.callback, false, generate_id()});
            do_fetch(static_cast<int>(pending_.size()) - 1);
            pending_.erase(pending_.begin() + index);
            return;
        }
    }

    if (pr.callback) {
        pr.callback(processed);
    }
    pending_.erase(pending_.begin() + index);
}

} // namespace hre::net
