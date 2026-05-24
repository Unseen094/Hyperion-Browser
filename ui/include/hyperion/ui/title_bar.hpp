#pragma once

#include <hyperion/ui/component.hpp>
#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/ui/dpi_manager.hpp>
#include <hyperion/ui/animation/animation_controller.hpp>

namespace hyperion::ui {

class title_bar : public component {
public:
    title_bar();
    void render(renderer& r) override;
    void handle_mouse_move(int x, int y) override;
    void handle_mouse_down(int x, int y) override;

    int height() const { return 36; }

    void on_close_click(std::function<void()> cb) { m_close_cb = std::move(cb); }
    void on_minimize_click(std::function<void()> cb) { m_minimize_cb = std::move(cb); }
    void on_maximize_click(std::function<void()> cb) { m_maximize_cb = std::move(cb); }

    bool hit_close(int x, int y) const;
    bool hit_minimize(int x, int y) const;
    bool hit_maximize(int x, int y) const;

private:
    void render_traffic_lights(renderer& r);
    void render_logo(renderer& r);
    void render_hyperion_ring(renderer& r, float cx, float cy, float radius);

    std::function<void()> m_close_cb;
    std::function<void()> m_minimize_cb;
    std::function<void()> m_maximize_cb;

    int m_hovered_btn = -1; // 0=close, 1=minimize, 2=maximize
    int m_width = 0;

    static constexpr float BTN_SIZE = 14.0f;
    static constexpr float BTN_GAP = 10.0f;
    static constexpr float BTN_X_START = 16.0f;
    static constexpr float BTN_Y = 11.0f;
};

} // namespace hyperion::ui
