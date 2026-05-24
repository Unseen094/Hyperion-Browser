#pragma once

#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/dpi_manager.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/ui/component.hpp>
#include <string>
#include <functional>
#include <memory>

namespace hyperion::ui {

struct weather_data {
    std::wstring location = L"--";
    double temp_c = 0;
    double temp_f = 32;
    std::wstring condition = L"Clear";
    std::wstring icon = L"01d";
    bool valid = false;
};

class weather_widget : public component {
public:
    weather_widget();
    ~weather_widget();

    void initialize(const std::wstring& api_key);
    void render(renderer& r) override;
    void handle_resize(int width, int height) override;
    void handle_mouse_down(int x, int y) override;

    void set_rect(const D2D1_RECT_F& rect) { m_rect = rect; }

    void refresh();

    bool has_data() const { return m_data.valid; }

private:
    void fetch_weather();
    void parse_response(const std::string& json);
    void render_card(renderer& r);
    void render_loading(renderer& r);

    std::wstring m_api_key;
    weather_data m_data;
    D2D1_RECT_F m_rect = {};
    bool m_loading = false;
    bool m_error = false;

    // Location
    double m_lat = 40.7128;
    double m_lon = -74.0060;
    bool m_location_acquired = false;
};

} // namespace hyperion::ui
