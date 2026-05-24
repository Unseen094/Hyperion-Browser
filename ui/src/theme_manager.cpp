#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/platform/logging.hpp>
#include <winreg.h>
#include <windows.h>

namespace hyperion::ui {

theme_manager::theme_manager() {
    if (detect_windows_dark_mode()) {
        build_dark_theme();
        m_current_theme = THEME_DARK;
    } else {
        build_light_theme();
    }
}

void theme_manager::set_theme(theme_type type) {
    m_current_theme = type;
    switch (type) {
        case THEME_LIGHT: build_light_theme(); break;
        case THEME_DARK: build_dark_theme(); break;
        case THEME_HIGH_CONTRAST: build_high_contrast_theme(); break;
        case THEME_SYSTEM:
            if (detect_windows_dark_mode()) {
                build_dark_theme();
            } else {
                build_light_theme();
            }
            break;
    }
    notify_changed();
}

std::wstring theme_manager::theme_name() const {
    switch (m_current_theme) {
        case THEME_LIGHT: return L"Light";
        case THEME_DARK: return L"Dark";
        case THEME_HIGH_CONTRAST: return L"High Contrast";
        case THEME_SYSTEM: return L"System";
    }
    return L"Light";
}

void theme_manager::build_light_theme() {
    m_colors.background = D2D1::ColorF(0.97f, 0.97f, 0.98f, 1.0f);
    m_colors.surface = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.surface_variant = D2D1::ColorF(0.95f, 0.95f, 0.97f, 1.0f);
    m_colors.text_primary = D2D1::ColorF(0.10f, 0.10f, 0.12f, 1.0f);
    m_colors.text_secondary = D2D1::ColorF(0.45f, 0.45f, 0.50f, 1.0f);
    m_colors.accent = D2D1::ColorF(0.10f, 0.48f, 0.92f, 1.0f);
    m_colors.accent_variant = D2D1::ColorF(0.06f, 0.37f, 0.76f, 1.0f);
    m_colors.error = D2D1::ColorF(0.90f, 0.25f, 0.25f, 1.0f);
    m_colors.divider = D2D1::ColorF(0.84f, 0.84f, 0.86f, 0.6f);
    m_colors.tab_active_bg = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.tab_inactive_bg = D2D1::ColorF(0.92f, 0.92f, 0.94f, 0.7f);
    m_colors.omnibox_bg = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.hover_overlay = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.06f);
    m_colors.shadow = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.08f);
    m_colors.bookmark_text = D2D1::ColorF(0.30f, 0.30f, 0.35f, 1.0f);
    m_colors.icon_active = D2D1::ColorF(0.20f, 0.20f, 0.23f, 1.0f);
    m_colors.icon_inactive = D2D1::ColorF(0.50f, 0.50f, 0.55f, 1.0f);
    m_colors.omnibox_radius = 18.0f;
    m_colors.corner_radius = 10.0f;
    m_colors.elevation_level = 1;

    m_colors.title_bar_bg = D2D1::ColorF(0.97f, 0.97f, 0.98f, 0.85f);
    m_colors.omnibox_pill_bg = D2D1::ColorF(0.92f, 0.92f, 0.94f, 0.9f);
    m_colors.omnibox_pill_border = D2D1::ColorF(0.80f, 0.80f, 0.84f, 0.5f);
    m_colors.glow_accent = D2D1::ColorF(0.10f, 0.48f, 0.92f, 0.15f);
    m_colors.card_bg = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.7f);
    m_colors.card_shadow = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.06f);
    m_colors.card_text = D2D1::ColorF(0.15f, 0.15f, 0.18f, 1.0f);
    m_colors.card_subtext = D2D1::ColorF(0.45f, 0.45f, 0.50f, 1.0f);
    m_colors.sidebar_bg = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.75f);
    m_colors.sidebar_accent = D2D1::ColorF(0.10f, 0.48f, 0.92f, 0.7f);
    m_colors.sidebar_header_text = D2D1::ColorF(0.15f, 0.15f, 0.18f, 1.0f);
    m_colors.sidebar_user_bubble = D2D1::ColorF(0.10f, 0.48f, 0.92f, 0.15f);
    m_colors.sidebar_ai_bubble = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.sidebar_user_text = D2D1::ColorF(0.10f, 0.48f, 0.92f, 1.0f);
    m_colors.sidebar_ai_text = D2D1::ColorF(0.15f, 0.15f, 0.18f, 1.0f);
    m_colors.sidebar_input_bg = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.sidebar_input_placeholder = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.bg_mesh_0 = D2D1::ColorF(0.90f, 0.92f, 0.98f, 1.0f);
    m_colors.bg_mesh_1 = D2D1::ColorF(0.85f, 0.92f, 0.98f, 1.0f);
    m_colors.bg_mesh_2 = D2D1::ColorF(0.95f, 0.88f, 0.98f, 1.0f);
    m_colors.bg_mesh_3 = D2D1::ColorF(0.88f, 0.95f, 0.97f, 1.0f);
    m_colors.bg_mesh_4 = D2D1::ColorF(0.97f, 0.93f, 0.90f, 1.0f);
    m_colors.logo_gradient_start = D2D1::ColorF(0.15f, 0.15f, 0.18f, 1.0f);
    m_colors.logo_gradient_end = D2D1::ColorF(0.42f, 0.67f, 0.95f, 1.0f);
    m_colors.omnibox_icon = D2D1::ColorF(0.45f, 0.45f, 0.50f, 1.0f);
    m_colors.omnibox_text_dimmed = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.omnibox_border = D2D1::ColorF(0.80f, 0.80f, 0.84f, 0.6f);
    m_colors.bookmark_bar_bg = D2D1::ColorF(0.97f, 0.97f, 0.98f, 1.0f);
    m_colors.bookmark_dot = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.bookmark_text_hover = D2D1::ColorF(0.10f, 0.10f, 0.12f, 1.0f);
}

void theme_manager::build_dark_theme() {
    m_colors.background = D2D1::ColorF(0.08f, 0.08f, 0.10f, 1.0f);
    m_colors.surface = D2D1::ColorF(0.14f, 0.14f, 0.16f, 1.0f);
    m_colors.surface_variant = D2D1::ColorF(0.18f, 0.18f, 0.20f, 1.0f);
    m_colors.text_primary = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.text_secondary = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.accent = D2D1::ColorF(0.42f, 0.67f, 0.95f, 1.0f);
    m_colors.accent_variant = D2D1::ColorF(0.31f, 0.56f, 0.88f, 1.0f);
    m_colors.error = D2D1::ColorF(0.90f, 0.35f, 0.35f, 1.0f);
    m_colors.divider = D2D1::ColorF(0.28f, 0.28f, 0.30f, 0.5f);
    m_colors.tab_active_bg = D2D1::ColorF(0.18f, 0.18f, 0.20f, 1.0f);
    m_colors.tab_inactive_bg = D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.6f);
    m_colors.omnibox_bg = D2D1::ColorF(0.22f, 0.22f, 0.25f, 1.0f);
    m_colors.hover_overlay = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.06f);
    m_colors.shadow = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.35f);
    m_colors.bookmark_text = D2D1::ColorF(0.70f, 0.70f, 0.75f, 1.0f);
    m_colors.icon_active = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.icon_inactive = D2D1::ColorF(0.50f, 0.50f, 0.55f, 1.0f);
    m_colors.omnibox_radius = 18.0f;
    m_colors.corner_radius = 10.0f;
    m_colors.elevation_level = 2;

    m_colors.title_bar_bg = D2D1::ColorF(0.08f, 0.08f, 0.10f, 0.85f);
    m_colors.omnibox_pill_bg = D2D1::ColorF(0.18f, 0.18f, 0.20f, 0.9f);
    m_colors.omnibox_pill_border = D2D1::ColorF(0.30f, 0.30f, 0.33f, 0.4f);
    m_colors.glow_accent = D2D1::ColorF(0.42f, 0.67f, 0.95f, 0.10f);
    m_colors.card_bg = D2D1::ColorF(0.16f, 0.16f, 0.18f, 0.7f);
    m_colors.card_shadow = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.25f);
    m_colors.card_text = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.card_subtext = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.sidebar_bg = D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.8f);
    m_colors.sidebar_accent = D2D1::ColorF(0.42f, 0.67f, 0.95f, 0.6f);
    m_colors.sidebar_header_text = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.sidebar_user_bubble = D2D1::ColorF(0.42f, 0.67f, 0.95f, 0.2f);
    m_colors.sidebar_ai_bubble = D2D1::ColorF(0.22f, 0.22f, 0.25f, 1.0f);
    m_colors.sidebar_user_text = D2D1::ColorF(0.42f, 0.67f, 0.95f, 1.0f);
    m_colors.sidebar_ai_text = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.sidebar_input_bg = D2D1::ColorF(0.18f, 0.18f, 0.20f, 1.0f);
    m_colors.sidebar_input_placeholder = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.bg_mesh_0 = D2D1::ColorF(0.06f, 0.06f, 0.12f, 1.0f);
    m_colors.bg_mesh_1 = D2D1::ColorF(0.04f, 0.08f, 0.18f, 1.0f);
    m_colors.bg_mesh_2 = D2D1::ColorF(0.12f, 0.04f, 0.18f, 1.0f);
    m_colors.bg_mesh_3 = D2D1::ColorF(0.04f, 0.12f, 0.15f, 1.0f);
    m_colors.bg_mesh_4 = D2D1::ColorF(0.14f, 0.08f, 0.06f, 1.0f);
    m_colors.logo_gradient_start = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    m_colors.logo_gradient_end = D2D1::ColorF(0.42f, 0.67f, 0.95f, 1.0f);
    m_colors.omnibox_icon = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.omnibox_text_dimmed = D2D1::ColorF(0.55f, 0.55f, 0.60f, 1.0f);
    m_colors.omnibox_border = D2D1::ColorF(0.30f, 0.30f, 0.33f, 0.5f);
    m_colors.bookmark_bar_bg = D2D1::ColorF(0.08f, 0.08f, 0.10f, 1.0f);
    m_colors.bookmark_dot = D2D1::ColorF(0.50f, 0.50f, 0.55f, 1.0f);
    m_colors.bookmark_text_hover = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
}

void theme_manager::build_high_contrast_theme() {
    m_colors.background = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.surface = D2D1::ColorF(0.05f, 0.05f, 0.05f, 1.0f);
    m_colors.surface_variant = D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f);
    m_colors.text_primary = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.text_secondary = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f);
    m_colors.accent = D2D1::ColorF(1.0f, 1.0f, 0.0f, 1.0f);
    m_colors.accent_variant = D2D1::ColorF(0.85f, 0.85f, 0.0f, 1.0f);
    m_colors.error = D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f);
    m_colors.divider = D2D1::ColorF(0.50f, 0.50f, 0.50f, 1.0f);
    m_colors.tab_active_bg = D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f);
    m_colors.tab_inactive_bg = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.omnibox_bg = D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f);
    m_colors.hover_overlay = D2D1::ColorF(0.30f, 0.30f, 0.30f, 1.0f);
    m_colors.shadow = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.50f);
    m_colors.bookmark_text = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.icon_active = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.icon_inactive = D2D1::ColorF(0.70f, 0.70f, 0.70f, 1.0f);
    m_colors.omnibox_radius = 4.0f;
    m_colors.corner_radius = 4.0f;
    m_colors.elevation_level = 0;
    m_colors.sidebar_header_text = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.sidebar_user_bubble = D2D1::ColorF(1.0f, 1.0f, 0.0f, 0.3f);
    m_colors.sidebar_ai_bubble = D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f);
    m_colors.sidebar_user_text = D2D1::ColorF(1.0f, 1.0f, 0.0f, 1.0f);
    m_colors.sidebar_ai_text = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.sidebar_input_bg = D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f);
    m_colors.sidebar_input_placeholder = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f);
    m_colors.bg_mesh_0 = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.bg_mesh_1 = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.bg_mesh_2 = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.bg_mesh_3 = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.bg_mesh_4 = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.logo_gradient_start = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.logo_gradient_end = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.omnibox_icon = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.omnibox_text_dimmed = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f);
    m_colors.omnibox_border = D2D1::ColorF(0.50f, 0.50f, 0.50f, 1.0f);
    m_colors.bookmark_bar_bg = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors.bookmark_dot = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    m_colors.bookmark_text_hover = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
}

void theme_manager::notify_changed() {
    for (auto& cb : m_theme_changed_callbacks) {
        if (cb) cb();
    }
}

bool theme_manager::detect_windows_dark_mode() const {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(DWORD);
        result = RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
            (LPBYTE)&value, &size);
        RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return value == 0;
        }
    }
    return false;
}

void theme_manager::apply_mica_backdrop() const {
    if (!m_hwnd) return;
    HMODULE dwmapi = GetModuleHandleW(L"dwmapi.dll");
    if (!dwmapi) return;
    auto DwmSetWindowAttribute = (HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD))
        GetProcAddress(dwmapi, "DwmSetWindowAttribute");
    if (!DwmSetWindowAttribute) return;
    BOOL enable = TRUE;
    DwmSetWindowAttribute(m_hwnd, 38, &enable, sizeof(enable));
    int mica = 2;
    DwmSetWindowAttribute(m_hwnd, 1029, &mica, sizeof(mica));
    HYPERION_LOG_INFO("Applied Mica backdrop effect");
}

void theme_manager::apply_acrylic_backdrop() const {
    if (!m_hwnd) return;
    HMODULE dwmapi = GetModuleHandleW(L"dwmapi.dll");
    if (!dwmapi) return;
    auto DwmSetWindowAttribute = (HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD))
        GetProcAddress(dwmapi, "DwmSetWindowAttribute");
    if (!DwmSetWindowAttribute) return;
    BOOL enable = TRUE;
    DwmSetWindowAttribute(m_hwnd, 38, &enable, sizeof(enable));
    HYPERION_LOG_INFO("Applied Acrylic backdrop effect");
}

void theme_manager::apply_rounded_corners() const {
    if (!m_hwnd) return;
    HMODULE dwmapi = GetModuleHandleW(L"dwmapi.dll");
    if (!dwmapi) return;
    auto DwmSetWindowAttribute = (HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD))
        GetProcAddress(dwmapi, "DwmSetWindowAttribute");
    if (!DwmSetWindowAttribute) return;
    int corner = 2;
    DwmSetWindowAttribute(m_hwnd, 33, &corner, sizeof(corner));
    HYPERION_LOG_INFO("Applied rounded corners");
}

} // namespace hyperion::ui
