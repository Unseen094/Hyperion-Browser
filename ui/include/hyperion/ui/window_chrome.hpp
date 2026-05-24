#pragma once

#include <hyperion/ui/renderer.hpp>
#include <windows.h>
#include <dwmapi.h>
#include <functional>
#include <string>

#pragma comment(lib, "dwmapi.lib")

namespace hyperion::ui {

enum class window_corner {
    rounded = 2,
    small_rounded = 1,
    square = 0,
};

struct window_backdrop {
    bool acrylic = true;
    bool mica = false;
    bool tab_effects = true;

    bool dark_mode = true;
    window_corner corner = window_corner::rounded;
    float corner_radius = 8.0f;
};

class window_chrome_manager {
public:
    static window_chrome_manager& instance();

    void apply(HWND hwnd);
    void update_theme(bool dark);
    HWND handle() const { return m_hwnd; }

    void set_corner_radius(float r) { m_config.corner_radius = r; }
    float corner_radius() const { return m_config.corner_radius; }

    window_backdrop& config() { return m_config; }

    // Custom caption rendering
    bool is_maximized() const;
    void handle_window_message(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    window_chrome_manager() = default;
    ~window_chrome_manager() = default;

    HWND m_hwnd = nullptr;
    window_backdrop m_config;

    void enable_acrylic(HWND hwnd);
    void set_corner_preference(HWND hwnd, window_corner corner);
    void set_titlebar_theme(HWND hwnd, bool dark);
};

} // namespace hyperion::ui
