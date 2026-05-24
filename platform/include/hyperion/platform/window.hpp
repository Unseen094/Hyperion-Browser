#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <functional>

namespace hyperion::platform {

using resize_callback = std::function<void(int width, int height)>;
using keyboard_callback = std::function<void(UINT key, bool is_down)>;
using char_callback = std::function<void(wchar_t c)>;
using mouse_callback = std::function<void(int x, int y, bool is_down)>;
using mouse_wheel_callback = std::function<void(float delta)>;
using activate_callback = std::function<void(bool focused)>;

struct window_config {
    std::wstring title = L"Hyperion Browser";
    int width = 1280;
    int height = 720;
};

class window {
public:
    explicit window(const window_config& config);
    ~window();

    // Disable copying
    window(const window&) = delete;
    window& operator=(const window&) = delete;

    void show() const;
    bool process_messages();
    void on_resize(resize_callback cb) { m_resize_cb = std::move(cb); }
    void on_key(keyboard_callback cb) { m_key_cb = std::move(cb); }
    void on_char(char_callback cb) { m_char_cb = std::move(cb); }
    void on_mouse(mouse_callback cb) { m_mouse_cb = std::move(cb); }
    void on_mouse_wheel(mouse_wheel_callback cb) { m_scroll_cb = std::move(cb); }
    void on_activate(activate_callback cb) { m_activate_cb = std::move(cb); }
    void on_nc_hit_test(std::function<LRESULT(POINT)> cb) { m_nc_hittest_cb = std::move(cb); }
    void on_nc_calc_size(std::function<void(bool, LPARAM)> cb) { m_nccalcsize_cb = std::move(cb); }
    
    HWND handle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    
    HWND m_hwnd = nullptr;
    HINSTANCE m_hinstance = nullptr;
    resize_callback m_resize_cb;
    keyboard_callback m_key_cb;
    char_callback m_char_cb;
    mouse_callback m_mouse_cb;
    mouse_wheel_callback m_scroll_cb;
    activate_callback m_activate_cb;
    std::function<LRESULT(POINT)> m_nc_hittest_cb;
    std::function<void(bool, LPARAM)> m_nccalcsize_cb;
};

} // namespace hyperion::platform
