#pragma once

#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/ui/dpi_manager.hpp>
#include <hyperion/ui/vector_icon.hpp>
#include <hyperion/ui/toolbar_fwd.hpp>
#include <hyperion/ui/animation/animation_controller.hpp>
#include <vector>
#include <string>
#include <functional>

namespace hyperion::ui {

class tab_renderer {
public:
    tab_renderer();

    void render(toolbar* toolbar, renderer& r, const std::vector<tab_ui_info>& tabs,
                float base_x, float base_y, float avail_width);

    void handle_mouse_down(int x, int y);
    void handle_mouse_move(int x, int y);

    void set_hovered_tab(int index) { m_hovered_tab = index; }
    int hovered_tab() const { return m_hovered_tab; }

    void on_tab_click(std::function<void(int)> cb) { m_tab_click_cb = std::move(cb); }
    void on_tab_close_click(std::function<void(int)> cb) { m_tab_close_cb = std::move(cb); }
    void on_new_tab_click(std::function<void()> cb) { m_new_tab_cb = std::move(cb); }

    float last_tab_end() const { return m_last_tab_end; }
    RECT last_plus_rect() const { return m_plus_rect; }
    RECT tab_rect_at(int index) const;
    RECT close_rect_at(int index) const;

private:
    void render_single_tab(renderer& r, const tab_ui_info& tab, const D2D1_RECT_F& rect,
                           float corner_radius, bool is_hovered);
    void render_plus_button(renderer& r, float x, float y);

    int m_hovered_tab = -1;
    float m_last_tab_end = 0;
    RECT m_plus_rect = {};
    std::function<void(int)> m_tab_click_cb;
    std::function<void(int)> m_tab_close_cb;
    std::function<void()> m_new_tab_cb;
    vector_icon_cache m_icons;
    std::vector<D2D1_RECT_F> m_tab_rects;
    std::vector<D2D1_RECT_F> m_close_rects;
};

} // namespace hyperion::ui
