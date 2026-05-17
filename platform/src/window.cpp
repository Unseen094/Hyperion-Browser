#include <hyperion/platform/window.hpp>
#include <hyperion/platform/logging.hpp>

namespace hyperion::platform {

namespace {
    const wchar_t* g_window_class_name = L"HyperionWindowClass";
}

window::window(const window_config& config) {
    m_hinstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = m_hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = g_window_class_name;

    if (!RegisterClassExW(&wc)) {
        HYPERION_LOG_ERROR("Failed to register window class");
        return;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        g_window_class_name,
        config.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        config.width,
        config.height,
        nullptr,
        nullptr,
        m_hinstance,
        this
    );

    if (!m_hwnd) {
        HYPERION_LOG_ERROR("Failed to create window");
        return;
    }

    HYPERION_LOG_INFO("Window created successfully");
}

window::~window() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
    UnregisterClassW(g_window_class_name, m_hinstance);
    HYPERION_LOG_INFO("Window destroyed");
}

void window::show() const {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
}

bool window::process_messages() {
    MSG msg = {0};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

LRESULT CALLBACK window::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCT*>(lparam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
    }

    auto* win = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    (void)win;

    switch (msg) {
        case WM_SIZE:
            if (win && win->m_resize_cb) {
                win->m_resize_cb(LOWORD(lparam), HIWORD(lparam));
            }
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (win && win->m_key_cb) {
                win->m_key_cb((UINT)wparam, true);
            }
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (win && win->m_key_cb) {
                win->m_key_cb((UINT)wparam, false);
            }
            return 0;
        case WM_CHAR:
            if (win && win->m_char_cb) {
                win->m_char_cb((wchar_t)wparam);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (win && win->m_mouse_cb) {
                win->m_mouse_cb(LOWORD(lparam), HIWORD(lparam), true);
            }
            return 0;
        case WM_LBUTTONUP:
            if (win && win->m_mouse_cb) {
                win->m_mouse_cb(LOWORD(lparam), HIWORD(lparam), false);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (win && win->m_mouse_cb) {
                // For mouse move, we'll pass false for is_down for now, 
                // or we could check MK_LBUTTON.
                win->m_mouse_cb(LOWORD(lparam), HIWORD(lparam), (wparam & MK_LBUTTON) != 0);
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (win && win->m_scroll_cb) {
                win->m_scroll_cb((float)GET_WHEEL_DELTA_WPARAM(wparam));
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

} // namespace hyperion::platform
