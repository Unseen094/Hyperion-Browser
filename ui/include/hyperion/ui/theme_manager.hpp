#pragma once

#include <cstdint>
#include <d2d1_3.h>
#include <dwmapi.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

namespace hyperion::ui {

enum theme_type : uint8_t {
    THEME_LIGHT,
    THEME_DARK,
    THEME_HIGH_CONTRAST,
    THEME_SYSTEM
};

struct theme_colors {
    theme_colors() : background(D2D1::ColorF(0,0,0,0)), surface(D2D1::ColorF(0,0,0,0)),
        surface_variant(D2D1::ColorF(0,0,0,0)), text_primary(D2D1::ColorF(0,0,0,0)),
        text_secondary(D2D1::ColorF(0,0,0,0)), accent(D2D1::ColorF(0,0,0,0)),
        accent_variant(D2D1::ColorF(0,0,0,0)), error(D2D1::ColorF(0,0,0,0)),
        divider(D2D1::ColorF(0,0,0,0)), tab_active_bg(D2D1::ColorF(0,0,0,0)),
        tab_inactive_bg(D2D1::ColorF(0,0,0,0)), omnibox_bg(D2D1::ColorF(0,0,0,0)),
        hover_overlay(D2D1::ColorF(0,0,0,0)), shadow(D2D1::ColorF(0,0,0,0)),
        bookmark_text(D2D1::ColorF(0,0,0,0)), icon_active(D2D1::ColorF(0,0,0,0)),
        icon_inactive(D2D1::ColorF(0,0,0,0)), omnibox_radius(0), corner_radius(0), elevation_level(0),
        title_bar_bg(D2D1::ColorF(0,0,0,0)), omnibox_pill_bg(D2D1::ColorF(0,0,0,0)),
        omnibox_pill_border(D2D1::ColorF(0,0,0,0)), glow_accent(D2D1::ColorF(0,0,0,0)),
        card_bg(D2D1::ColorF(0,0,0,0)), card_shadow(D2D1::ColorF(0,0,0,0)),
        card_text(D2D1::ColorF(0,0,0,0)), card_subtext(D2D1::ColorF(0,0,0,0)),
        sidebar_bg(D2D1::ColorF(0,0,0,0)), sidebar_accent(D2D1::ColorF(0,0,0,0)),
        sidebar_header_text(D2D1::ColorF(0,0,0,0)), sidebar_user_bubble(D2D1::ColorF(0,0,0,0)),
        sidebar_ai_bubble(D2D1::ColorF(0,0,0,0)), sidebar_user_text(D2D1::ColorF(0,0,0,0)),
        sidebar_ai_text(D2D1::ColorF(0,0,0,0)), sidebar_input_bg(D2D1::ColorF(0,0,0,0)),
        sidebar_input_placeholder(D2D1::ColorF(0,0,0,0)),
        bg_mesh_0(D2D1::ColorF(0,0,0,0)), bg_mesh_1(D2D1::ColorF(0,0,0,0)),
        bg_mesh_2(D2D1::ColorF(0,0,0,0)), bg_mesh_3(D2D1::ColorF(0,0,0,0)),
        bg_mesh_4(D2D1::ColorF(0,0,0,0)),
        logo_gradient_start(D2D1::ColorF(0,0,0,0)), logo_gradient_end(D2D1::ColorF(0,0,0,0)),
        omnibox_icon(D2D1::ColorF(0,0,0,0)), omnibox_text_dimmed(D2D1::ColorF(0,0,0,0)),
        omnibox_border(D2D1::ColorF(0,0,0,0)),
        bookmark_bar_bg(D2D1::ColorF(0,0,0,0)), bookmark_dot(D2D1::ColorF(0,0,0,0)),
        bookmark_text_hover(D2D1::ColorF(0,0,0,0)) {}
    D2D1::ColorF background;
    D2D1::ColorF surface;
    D2D1::ColorF surface_variant;
    D2D1::ColorF text_primary;
    D2D1::ColorF text_secondary;
    D2D1::ColorF accent;
    D2D1::ColorF accent_variant;
    D2D1::ColorF error;
    D2D1::ColorF divider;
    D2D1::ColorF tab_active_bg;
    D2D1::ColorF tab_inactive_bg;
    D2D1::ColorF omnibox_bg;
    D2D1::ColorF hover_overlay;
    D2D1::ColorF shadow;
    D2D1::ColorF bookmark_text;
    D2D1::ColorF icon_active;
    D2D1::ColorF icon_inactive;
    float omnibox_radius;
    float corner_radius;
    int elevation_level;
    D2D1::ColorF title_bar_bg;
    D2D1::ColorF omnibox_pill_bg;
    D2D1::ColorF omnibox_pill_border;
    D2D1::ColorF glow_accent;
    D2D1::ColorF card_bg;
    D2D1::ColorF card_shadow;
    D2D1::ColorF card_text;
    D2D1::ColorF card_subtext;
    D2D1::ColorF sidebar_bg;
    D2D1::ColorF sidebar_accent;
    D2D1::ColorF sidebar_header_text;
    D2D1::ColorF sidebar_user_bubble;
    D2D1::ColorF sidebar_ai_bubble;
    D2D1::ColorF sidebar_user_text;
    D2D1::ColorF sidebar_ai_text;
    D2D1::ColorF sidebar_input_bg;
    D2D1::ColorF sidebar_input_placeholder;
    D2D1::ColorF bg_mesh_0;
    D2D1::ColorF bg_mesh_1;
    D2D1::ColorF bg_mesh_2;
    D2D1::ColorF bg_mesh_3;
    D2D1::ColorF bg_mesh_4;
    D2D1::ColorF logo_gradient_start;
    D2D1::ColorF logo_gradient_end;
    D2D1::ColorF omnibox_icon;
    D2D1::ColorF omnibox_text_dimmed;
    D2D1::ColorF omnibox_border;
    D2D1::ColorF bookmark_bar_bg;
    D2D1::ColorF bookmark_dot;
    D2D1::ColorF bookmark_text_hover;
};


class theme_manager {
public:
    static theme_manager& instance() {
        static theme_manager inst;
        return inst;
    }

    theme_manager();
    ~theme_manager() = default;

    void set_theme(theme_type type);
    theme_type current_theme() const { return m_current_theme; }
    const theme_colors& colors() const { return m_colors; }
    void set_hwnd(HWND hwnd) { m_hwnd = hwnd; }
    void apply_mica_backdrop() const;
    void apply_acrylic_backdrop() const;
    void apply_rounded_corners() const;
    std::wstring theme_name() const;
    bool is_dark() const { return m_current_theme == THEME_DARK || m_current_theme == THEME_HIGH_CONTRAST; }
    void on_theme_changed(std::function<void()> cb) { m_theme_changed_callbacks.push_back(std::move(cb)); }

private:
    void build_light_theme();
    void build_dark_theme();
    void build_high_contrast_theme();
    void notify_changed();
    bool detect_windows_dark_mode() const;

    theme_type m_current_theme = THEME_LIGHT;
    theme_colors m_colors;
    HWND m_hwnd = nullptr;
    std::vector<std::function<void()>> m_theme_changed_callbacks;
};

} // namespace hyperion::ui
