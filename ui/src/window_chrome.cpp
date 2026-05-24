#include <hyperion/ui/window_chrome.hpp>
#include <hyperion/platform/logging.hpp>
#include <d2d1_1.h>
#include <d3d11.h>
#include <versionhelpers.h>
#include <dwmapi.h>
#include <windowsx.h>

// SDK compatibility definitions for Windows 10 1809+ features
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_MICA_ENABLED
#define DWMWA_MICA_ENABLED 1029
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWCP_ROUNDSMALL
#define DWMWCP_ROUNDSMALL 1
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 0
#endif

// ACCENT_POLICY for acrylic blur
#ifndef ACCENT_ENABLE_BLURBEHIND
#define ACCENT_ENABLE_BLURBEHIND 3
#endif
#ifndef WCA_ACCENT_POLICY
#define WCA_ACCENT_POLICY 19
#endif

#ifndef ACCENT_POLICY_DEFINED
#define ACCENT_POLICY_DEFINED
typedef struct _ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
} ACCENT_POLICY;
#endif

#ifndef WINDOWCOMPOSITIONATTRIBDATA_DEFINED
#define WINDOWCOMPOSITIONATTRIBDATA_DEFINED
typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;
#endif

#ifndef SetWindowCompositionAttribute_Defined
typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
#endif

namespace hyperion::ui {

window_chrome_manager& window_chrome_manager::instance() {
    static window_chrome_manager mgr;
    return mgr;
}

// Original window proc saved for subclassing
static WNDPROC g_original_wndproc = nullptr;

static LRESULT CALLBACK chrome_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                NCCALCSIZE_PARAMS* ncp = (NCCALCSIZE_PARAMS*)lParam;
                ncp->rgrc[0].top = 0;
                return 0;
            }
            return 0;
        }

        case WM_NCHITTEST: {
            // Don't handle - let the original window_proc's m_nc_hittest_cb deal with it
            return CallWindowProc(g_original_wndproc, hwnd, msg, wParam, lParam);
        }
    }

    return CallWindowProc(g_original_wndproc, hwnd, msg, wParam, lParam);
}

void window_chrome_manager::apply(HWND hwnd) {
    m_hwnd = hwnd;

    if (IsWindows10OrGreater()) {
        enable_acrylic(hwnd);
        set_corner_preference(hwnd, m_config.corner);
    }

    set_titlebar_theme(hwnd, m_config.dark_mode);

    // Remove standard titlebar and subclass window for custom chrome
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~WS_CAPTION;
    style |= WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_SIZEBOX;
    SetWindowLong(hwnd, GWL_STYLE, style);

    // Apply the frame change immediately
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    if (!g_original_wndproc) {
        g_original_wndproc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)chrome_wndproc);
    }
}

void window_chrome_manager::update_theme(bool dark) {
    m_config.dark_mode = dark;
    if (m_hwnd) {
        set_titlebar_theme(m_hwnd, dark);
    }
}

bool window_chrome_manager::is_maximized() const {
    if (!m_hwnd) return false;
    WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    GetWindowPlacement(m_hwnd, &wp);
    return wp.showCmd == SW_MAXIMIZE;
}

void window_chrome_manager::enable_acrylic(HWND hwnd) {
    if (!IsWindows10OrGreater()) return;

    ACCENT_POLICY accent = {};
    accent.AccentState = ACCENT_ENABLE_BLURBEHIND;
    accent.AccentFlags = 0x20;
    accent.GradientColor = 0x99000000;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &accent;
    data.cbData = sizeof(accent);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    auto fn = (pSetWindowCompositionAttribute)GetProcAddress(user32, "SetWindowCompositionAttribute");
    if (fn) {
        fn(hwnd, &data);
    }
}

void window_chrome_manager::set_corner_preference(HWND hwnd, window_corner corner) {
    if (!IsWindows10OrGreater()) return;

    int pref = DWMWCP_ROUND;
    switch (corner) {
        case window_corner::rounded: pref = DWMWCP_ROUND; break;
        case window_corner::small_rounded: pref = DWMWCP_ROUNDSMALL; break;
        case window_corner::square: pref = DWMWCP_DONOTROUND; break;
    }
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &pref, sizeof(pref));
}

void window_chrome_manager::set_titlebar_theme(HWND hwnd, bool dark) {
    if (!IsWindows10OrGreater()) return;

    BOOL use_dark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &use_dark, sizeof(use_dark));
}

void window_chrome_manager::handle_window_message(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SETTINGCHANGE:
            if (wParam == 0 && lParam != 0) {
                std::wstring setting = (const wchar_t*)lParam;
                if (setting == L"ImmersiveColorSet") {
                    // Theme changed, re-apply
                    if (m_hwnd) {
                        apply(m_hwnd);
                    }
                }
            }
            break;
        case WM_DWMCOLORIZATIONCOLORCHANGED:
            // Accent color changed
            break;
        default:
            break;
    }
}

} // namespace hyperion::ui
