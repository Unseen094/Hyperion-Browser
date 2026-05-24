#pragma once

#include <hyperion/ui/component.hpp>
#include <hyperion/ui/vector_icon.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/dpi_manager.hpp>
#include <dwrite.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <chrono>

namespace hyperion::ui {

class omnibox_edit : public component {
public:
    omnibox_edit();
    void render(renderer& r) override;
    void handle_char(wchar_t c) override;
    void handle_mouse_down(int x, int y) override;
    void handle_resize(int width, int height) override;

    const std::wstring& text() const { return m_text; }
    void set_text(const std::wstring& t);
    void set_placeholder(const std::wstring& p) { m_placeholder = p; }
    void set_omnibox_rect(const D2D1_RECT_F& r) { m_omni_rect = r; }
    void set_focused(bool f) { m_focused = f; }
    bool is_focused() const { return m_focused; }
    void set_secure(bool s) { m_is_secure = s; }

    D2D1_RECT_F omnibox_rect() const { return m_omni_rect; }

    void on_text_changed(std::function<void(const std::wstring&)> cb) { m_text_changed_cb = std::move(cb); }
    void on_enter(std::function<void(const std::wstring&)> cb) { m_enter_cb = std::move(cb); }

    float desired_height() const { return 38.0f; }

    // Hit test for right side buttons
    bool hit_extensions(int x, int y) const;
    bool hit_profile(int x, int y) const;
    bool hit_settings(int x, int y) const;
    bool hit_ai_shortcut(int x, int y) const;
    bool hit_downloads(int x, int y) const;

    void on_extensions_click(std::function<void()> cb) { m_ext_cb = std::move(cb); }
    void on_profile_click(std::function<void()> cb) { m_profile_cb = std::move(cb); }
    void on_settings_click(std::function<void()> cb) { m_settings_cb = std::move(cb); }
    void on_ai_click(std::function<void()> cb) { m_ai_cb = std::move(cb); }
    void on_downloads_click(std::function<void()> cb) { m_dl_cb = std::move(cb); }

private:
    void render_pill_bg(renderer& r);
    void render_left_icons(renderer& r);
    void render_lock_icon(renderer& r);
    void render_text(renderer& r);
    void render_cursor(renderer& r, const D2D1_RECT_F& text_rect, const D2D1_RECT_F& omni);
    void render_cursor_from_layout(renderer& r, IDWriteTextLayout* layout, const D2D1_RECT_F& text_rect, const D2D1_RECT_F& omni);
    void render_right_icons(renderer& r);
    void render_star(renderer& r);

    std::wstring m_text;
    std::wstring m_placeholder = L"Search the web or ask Hyperion AI\u2026";
    D2D1_RECT_F m_omni_rect = {};
    bool m_focused = false;
    bool m_is_secure = false;
    int m_cursor_pos = 0;
    float m_focus_anim = 0.0f;

    std::function<void(const std::wstring&)> m_text_changed_cb;
    std::function<void(const std::wstring&)> m_enter_cb;
    std::function<void()> m_ext_cb;
    std::function<void()> m_profile_cb;
    std::function<void()> m_settings_cb;
    std::function<void()> m_ai_cb;
    std::function<void()> m_dl_cb;

    std::chrono::steady_clock::time_point m_cursor_blink_start;
    bool m_cursor_visible = true;
};

struct suggestion_entry {
    std::wstring text;
    std::wstring url;
    std::wstring subtitle;
    enum type { SEARCH, URL, BOOKMARK, HISTORY } entry_type = SEARCH;
};

class suggestion_popup : public component {
public:
    suggestion_popup();
    void render(renderer& r) override;
    void handle_mouse_move(int x, int y) override;
    void handle_mouse_down(int x, int y) override;
    void handle_resize(int width, int height) override;

    void set_suggestions(const std::vector<suggestion_entry>& suggestions);
    void set_anchor_rect(const D2D1_RECT_F& rect) { m_anchor_rect = rect; }
    bool is_visible() const { return m_visible; }
    void set_visible(bool v) { m_visible = v; }

    void on_suggestion_click(std::function<void(const suggestion_entry&)> cb) { m_click_cb = std::move(cb); }

private:
    std::vector<suggestion_entry> m_suggestions;
    D2D1_RECT_F m_anchor_rect = {};
    bool m_visible = false;
    int m_hovered_index = -1;
    std::function<void(const suggestion_entry&)> m_click_cb;
};

} // namespace hyperion::ui
