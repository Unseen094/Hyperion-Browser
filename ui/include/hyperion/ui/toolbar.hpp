#pragma once

#include <hyperion/ui/component.hpp>
#include <hyperion/ui/dpi_manager.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/ui/vector_icon.hpp>
#include <hyperion/ui/omnibox.hpp>
#include <hyperion/ui/toolbar_fwd.hpp>
#include <hyperion/ui/tab_renderer.hpp>
#include <hyperion/ui/modern_bookmarks_bar.hpp>
#include <hyperion/ui/effects/glass_effect.hpp>
#include <hyperion/ui/animation/animation_controller.hpp>
#include <hyperion/ui/accessibility_handler.hpp>
#include <hyperion/ui/new_tab_dashboard.hpp>
#include <hyperion/ui/ai_sidebar.hpp>
#include <hyperion/ui/window_chrome.hpp>
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace hyperion::ui {

class toolbar : public component {
public:
    toolbar();
    ~toolbar();

    void initialize(HWND hwnd, renderer& r);
    void render(renderer& r) override;
    void handle_resize(int width, int height) override;
    void handle_char(wchar_t c) override;
    void handle_key_down(UINT vk) override;
    void handle_mouse_move(int x, int y) override;
    void handle_mouse_down(int x, int y) override;

    const std::wstring& address() const { return m_omnibox->text(); }
    void set_address(const std::wstring& address);
    
    void set_tabs(const std::vector<tab_ui_info>& tabs) { m_tabs = tabs; }
    void on_tab_click(std::function<void(size_t)> cb) { m_tab_click_cb = std::move(cb); }
    void on_tab_close(std::function<void(size_t)> cb) { m_tab_close_cb = std::move(cb); }
    void on_new_tab_click(std::function<void()> cb) { m_new_tab_click_cb = std::move(cb); }
    
    void on_back_click(std::function<void()> cb) { m_back_cb = std::move(cb); }
    void on_forward_click(std::function<void()> cb) { m_forward_cb = std::move(cb); }
    void on_refresh_click(std::function<void()> cb) { m_refresh_cb = std::move(cb); }
    void on_home_click(std::function<void()> cb) { m_home_cb = std::move(cb); }
    void on_add_bookmark_click(std::function<void(const std::wstring&, const std::wstring&)> cb) { m_add_bookmark_cb = std::move(cb); }

    void set_navigation_state(bool can_back, bool can_forward);
    void set_progress(double progress);
    void set_theme(theme_type type);
    void animate_tab_close(int index);
    void animate_tab_open();

    omnibox_edit& omnibox() { return *m_omnibox; }
    suggestion_popup& suggestions() { return *m_suggestions; }
    modern_bookmarks_bar& bookmarks_bar() { return m_bookmarks_bar; }
    vector_icon_cache& icons() { return m_icons; }
    glass_effect& effects() { return m_glass; }

    int toolbar_height() const { return m_height; }
    int total_ui_height() const { return m_height + m_bookmarks_bar.height(); }

    void on_omnibox_enter(std::function<void(const std::wstring&)> cb) { m_omnibox_enter_cb = std::move(cb); }

    void show_new_tab_dashboard();
    void hide_dashboard();
    void toggle_ai_sidebar();
    bool is_dashboard_showing() const { return m_show_dashboard; }
    bool sidebar_open() const { return m_sidebar_open; }
    float sidebar_anim_progress() const { return m_sidebar_anim; }
    bool is_point_interactive(int x, int y) const;
    void set_weather_api_key(const std::wstring& key) { m_weather_api_key = key; }

private:
    void render_nav_buttons(renderer& r);
    void render_omnibox_area(renderer& r);
    void render_tab_strip(renderer& r);
    void render_security_badge(renderer& r);

    int m_width = 0;
    int m_height = 100;
    double m_progress = 0.0;
    std::vector<tab_ui_info> m_tabs;

    std::function<void(size_t)> m_tab_click_cb;
    std::function<void(size_t)> m_tab_close_cb;
    std::function<void()> m_new_tab_click_cb;
    std::function<void()> m_back_cb;
    std::function<void()> m_forward_cb;
    std::function<void()> m_refresh_cb;
    std::function<void()> m_home_cb;
    std::function<void(const std::wstring&, const std::wstring&)> m_add_bookmark_cb;
    std::function<void(const std::wstring&)> m_omnibox_enter_cb;

    bool m_can_go_back = false;
    bool m_can_go_forward = false;
    bool m_initialized = false;

    // Modern components
    std::unique_ptr<omnibox_edit> m_omnibox;
    std::unique_ptr<suggestion_popup> m_suggestions;
    std::unique_ptr<tab_renderer> m_tab_renderer;
    modern_bookmarks_bar m_bookmarks_bar;
    vector_icon_cache m_icons;
    glass_effect m_glass;

    RECT m_back_rect = {0,0,0,0};
    RECT m_forward_rect = {0,0,0,0};
    RECT m_refresh_rect = {0,0,0,0};
    RECT m_home_rect = {0,0,0,0};
    RECT m_star_rect = {0,0,0,0};
    RECT m_omni_rect = {0,0,0,0};

    // New components
    std::unique_ptr<new_tab_dashboard> m_new_tab;
    std::unique_ptr<ai_sidebar> m_ai_sidebar;
    window_chrome_manager* m_window_chrome = nullptr;

    std::wstring m_weather_api_key;
    bool m_sidebar_open = false;
    float m_sidebar_anim = 0.0f;
    float m_dashboard_fade = 0.0f;
    bool m_show_dashboard = false;
    int m_window_height = 720;
};

} // namespace hyperion::ui
