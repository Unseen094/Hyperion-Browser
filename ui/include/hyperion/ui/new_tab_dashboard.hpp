#pragma once

#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/weather_widget.hpp>
#include <hyperion/ui/component.hpp>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace hyperion::ui {

struct speed_dial_entry {
    std::wstring title;
    std::wstring url;
    std::wstring favicon_url;
};

class new_tab_dashboard : public component {
public:
    new_tab_dashboard();
    ~new_tab_dashboard();

    void initialize(const std::wstring& weather_api_key);
    void render(renderer& r) override;
    void handle_resize(int width, int height) override;
    void handle_mouse_down(int x, int y) override;
    void handle_mouse_move(int x, int y) override;
    bool hit_test(int x, int y) const;

    void set_visible(bool v) { m_visible = v; }
    bool visible() const { return m_visible; }
    void set_rect(const D2D1_RECT_F& rect) { m_rect = rect; }

    void set_navigate_callback(std::function<void(const std::wstring&)> cb) {
        m_navigate_cb = std::move(cb);
    }

private:
    void render_background(renderer& r);
    void render_search_bar(renderer& r);
    void render_speed_dial(renderer& r);
    void render_logo(renderer& r);

    void add_default_speed_dials();

    std::unique_ptr<weather_widget> m_weather;
    std::vector<speed_dial_entry> m_speed_dials;
    D2D1_RECT_F m_rect = {};
    D2D1_RECT_F m_search_rect = {};
    bool m_visible = false;
    int m_hovered_card = -1;
    std::function<void(const std::wstring&)> m_navigate_cb;
};

} // namespace hyperion::ui
