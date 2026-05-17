#pragma once

#include <hyperion/ui/renderer.hpp>
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace hyperion::ui {

class component {
public:
    virtual ~component() = default;
    virtual void render(renderer& r) = 0;
    virtual void handle_mouse_move(int x, int y) { (void)x; (void)y; }
    virtual void handle_mouse_down(int x, int y) { (void)x; (void)y; }
    virtual void handle_char(wchar_t c) { (void)c; }
    virtual void handle_resize(int width, int height) { (void)width; (void)height; }
};

struct tab_ui_info {
    std::wstring title;
    bool active;
    RECT rect;
    RECT close_rect;
};

class toolbar : public component {
public:
    toolbar();
    void render(renderer& r) override;
    void handle_resize(int width, int height) override;
    void handle_char(wchar_t c) override;
    void handle_mouse_down(int x, int y) override;

    const std::wstring& address() const { return m_address; }
    void set_address(const std::wstring& address) { m_address = address; }
    
    void set_tabs(const std::vector<tab_ui_info>& tabs) { m_tabs = tabs; }
    void on_tab_click(std::function<void(size_t)> cb) { m_tab_click_cb = std::move(cb); }
    void on_tab_close(std::function<void(size_t)> cb) { m_tab_close_cb = std::move(cb); }
    void on_new_tab_click(std::function<void()> cb) { m_new_tab_click_cb = std::move(cb); }
    
    void on_back_click(std::function<void()> cb) { m_back_cb = std::move(cb); }
    void on_forward_click(std::function<void()> cb) { m_forward_cb = std::move(cb); }
    void on_refresh_click(std::function<void()> cb) { m_refresh_cb = std::move(cb); }
    void on_home_click(std::function<void()> cb) { m_home_cb = std::move(cb); }
    void on_add_bookmark_click(std::function<void(const std::wstring&, const std::wstring&)> cb) { m_add_bookmark_cb = std::move(cb); }

    void set_navigation_state(bool can_back, bool can_forward) { 
        m_can_go_back = can_back; 
        m_can_go_forward = can_forward; 
    }

    void set_progress(double progress) { m_progress = progress; }

private:
    int m_width = 0;
    int m_height = 80;
    std::wstring m_address = L"https://www.google.com";
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

    bool m_can_go_back = false;
    bool m_can_go_forward = false;

    RECT m_back_rect = {0,0,0,0};
    RECT m_forward_rect = {0,0,0,0};
    RECT m_refresh_rect = {0,0,0,0};
    RECT m_home_rect = {0,0,0,0};
    RECT m_star_rect = {0,0,0,0};
};

} // namespace hyperion::ui
