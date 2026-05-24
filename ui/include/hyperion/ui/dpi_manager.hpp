#pragma once

#include <d2d1_3.h>
#include <windows.h>
#include <shellscalingapi.h>
#include <unordered_map>

#pragma comment(lib, "Shcore.lib")

namespace hyperion::ui {

class dpi_manager {
public:
    static dpi_manager& instance() {
        static dpi_manager inst;
        return inst;
    }

    void initialize(HWND hwnd) {
        m_hwnd = hwnd;
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        update_dpi();
    }

    void update_dpi() {
        UINT dpi = GetDpiForWindow(m_hwnd);
        if (dpi != m_current_dpi) {
            m_current_dpi = dpi;
            m_scale_x = dpi / 96.0f;
            m_scale_y = dpi / 96.0f;
        }
    }

    UINT dpi() const { return m_current_dpi; }
    float scale_x() const { return m_scale_x; }
    float scale_y() const { return m_scale_y; }
    float scale() const { return (m_scale_x + m_scale_y) / 2.0f; }

    int scale_px(int px) const { return (int)(px * m_scale_x + 0.5f); }
    float scale_px(float px) const { return px * m_scale_x; }

    RECT scale_rect(const RECT& r) const {
        return {
            (int)(r.left * m_scale_x),
            (int)(r.top * m_scale_y),
            (int)(r.right * m_scale_x),
            (int)(r.bottom * m_scale_y)
        };
    }

    D2D1_RECT_F scale_rect(const D2D1_RECT_F& r) const {
        return D2D1::RectF(
            r.left * m_scale_x,
            r.top * m_scale_y,
            r.right * m_scale_x,
            r.bottom * m_scale_y
        );
    }

    int dpi_aware_value(int pixel_value, int base_dpi = 96) const {
        return MulDiv(pixel_value, (int)m_current_dpi, base_dpi);
    }

private:
    dpi_manager() : m_current_dpi(96), m_scale_x(1.0f), m_scale_y(1.0f), m_hwnd(nullptr) {}
    HWND m_hwnd;
    UINT m_current_dpi;
    float m_scale_x;
    float m_scale_y;
};

} // namespace hyperion::ui
