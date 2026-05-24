#include <hyperion/ui/ai_client.hpp>
#include <hyperion/platform/logging.hpp>
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace hyperion::ui {

static std::string url_encode(const std::string& s) {
    // Minimal JSON-safe string — no URL encoding needed for JSON body
    return s;
}

static std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

ai_client::ai_client() = default;
ai_client::~ai_client() = default;

void ai_client::initialize(const std::string& api_key, const std::string& model) {
    m_api_key = api_key;
    if (!model.empty()) m_model = model;
}

void ai_client::clear_history() {
    m_history.clear();
}

void ai_client::set_system_prompt(const std::string& prompt) {
    m_system_prompt = prompt;
}

void ai_client::send_message(const std::string& user_message,
                              std::function<void(const std::string&)> on_response,
                              std::function<void(const std::string&)> on_error) {
    m_history.push_back({"user", user_message});

    // Build JSON body
    std::ostringstream body;
    body << R"({"model":")" << escape_json(m_model) << R"(","messages":[
        {"role":"system","content":")" << escape_json(m_system_prompt) << R"("})";

    for (const auto& msg : m_history) {
        body << R"(,{"role":")" << msg.role << R"(","content":")" << escape_json(msg.content) << R"("})";
    }

    body << R"(],"max_tokens":1024})";

    do_request(body.str(), on_response, on_error);
}

void ai_client::do_request(const std::string& body,
                            std::function<void(const std::string&)> on_response,
                            std::function<void(const std::string&)> on_error) {
    std::thread([this, body, on_response = std::move(on_response), on_error = std::move(on_error)]() {
        HINTERNET hSession = WinHttpOpen(L"Hyperion/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
        if (!hSession) {
            if (on_error) on_error("Failed to open HTTP session");
            return;
        }

        std::wstring host(m_base_url.begin(), m_base_url.end());
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            if (on_error) on_error("Failed to connect");
            return;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
            L"/v1/chat/completions", nullptr, nullptr, nullptr,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (on_error) on_error("Failed to open request");
            return;
        }

        // Set headers
        std::wstring auth = L"Authorization: Bearer " + std::wstring(m_api_key.begin(), m_api_key.end());
        LPCWSTR headers[] = {
            auth.c_str(),
            L"Content-Type: application/json",
        };

        WinHttpAddRequestHeaders(hRequest, headers[1], (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(hRequest, headers[0], (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
        if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

        std::string response;
        if (ok) {
            DWORD bytes = 0;
            while (WinHttpQueryDataAvailable(hRequest, &bytes) && bytes > 0) {
                std::vector<char> buf(bytes + 1, 0);
                DWORD read = 0;
                WinHttpReadData(hRequest, buf.data(), bytes, &read);
                response.append(buf.data(), read);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (!ok || response.empty()) {
            if (on_error) on_error("Request failed");
            return;
        }

        // Parse JSON response for content
        // Look for "content":"..." in the response
        auto find_content = [](const std::string& json) -> std::string {
            auto search = R"("content":")";
            auto pos = json.find(search);
            if (pos == std::string::npos) return {};
            pos += strlen(search);
            std::string result;
            bool escape = false;
            for (; pos < json.size(); pos++) {
                if (escape) {
                    if (json[pos] == 'n') result += '\n';
                    else if (json[pos] == 't') result += '\t';
                    else if (json[pos] == 'r') result += '\r';
                    else if (json[pos] == '"') result += '"';
                    else if (json[pos] == '\\') result += '\\';
                    else result += json[pos];
                    escape = false;
                } else if (json[pos] == '\\') {
                    escape = true;
                } else if (json[pos] == '"') {
                    break;
                } else {
                    result += json[pos];
                }
            }
            return result;
        };

        std::string content = find_content(response);
        if (!content.empty()) {
            m_history.push_back({"assistant", content});
            if (on_response) on_response(content);
        } else {
            if (on_error) on_error("Empty response from AI");
        }
    }).detach();
}

} // namespace hyperion::ui
