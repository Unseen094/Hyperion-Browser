#pragma once

#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/component.hpp>
#include <hyperion/ui/ai_client.hpp>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <functional>

namespace hyperion::ui {

struct chat_message {
    bool from_user;
    std::wstring text;
};

class ai_sidebar : public component {
public:
    ai_sidebar();
    ~ai_sidebar();

    void render(renderer& r) override;
    void handle_resize(int width, int height) override;
    void handle_mouse_down(int x, int y) override;
    void handle_mouse_move(int x, int y) override;
    void handle_char(wchar_t c) override;
    void handle_key_down(UINT vk) override;

    void set_sidebar_width(float w) { m_sidebar_w = w; }
    float sidebar_width() const { return m_sidebar_w; }

    void toggle();
    void open();
    void close();
    bool is_open() const { return m_open; }

    float animation_progress() const { return m_anim_progress; }
    void set_animation_progress(float p) { m_anim_progress = p; }

    void initialize_ai(const std::string& api_key, const std::string& model = "meta/llama-3.1-8b-instruct");
    void add_message(bool from_user, const std::wstring& text);

    int input_height() const { return (int)m_input_height; }

private:
    void render_backdrop(renderer& r);
    void render_panel(renderer& r);
    void render_header(renderer& r);
    void render_messages(renderer& r);
    void render_input(renderer& r);
    void render_toggle_button(renderer& r);

    void send_to_ai();

    static constexpr float PANEL_W = 340.0f;

    bool m_open = false;
    float m_sidebar_w = PANEL_W;
    float m_anim_progress = 0.0f;
    int m_hovered_close = false;

    float m_input_height = 0.0f;

    std::deque<chat_message> m_messages;
    std::unique_ptr<ai_client> m_ai;
    std::wstring m_input_text;
    bool m_waiting_for_ai = false;
    int m_width = 0;
    int m_height = 0;
};

} // namespace hyperion::ui
