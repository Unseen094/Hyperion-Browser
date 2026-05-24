#pragma once

#include <string>
#include <functional>
#include <deque>

namespace hyperion::ui {

struct ai_chat_message {
    std::string role; // "user" or "assistant"
    std::string content;
};

class ai_client {
public:
    ai_client();
    ~ai_client();

    void initialize(const std::string& api_key, const std::string& model);
    void send_message(const std::string& user_message,
                      std::function<void(const std::string&)> on_response,
                      std::function<void(const std::string&)> on_error);

    void clear_history();
    void set_system_prompt(const std::string& prompt);

private:
    void do_request(const std::string& body,
                    std::function<void(const std::string&)> on_response,
                    std::function<void(const std::string&)> on_error);

    std::string m_api_key;
    std::string m_model = "meta/llama-3.1-8b-instruct";
    std::string m_base_url = "integrate.api.nvidia.com";
    std::string m_system_prompt = "You are a helpful AI assistant integrated into the Hyperion browser. Be concise and accurate.";
    std::deque<ai_chat_message> m_history;
};

} // namespace hyperion::ui
