#pragma once

#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <memory>

namespace hyperion::ui {

class glass_effect {
public:
    glass_effect();
    ~glass_effect();

    bool initialize(renderer& r);
    void apply_backdrop_blur(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect, float blur_radius = 10.0f);
    void apply_drop_shadow(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect, float radius = 8.0f);
    void begin_glass_layer(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect, float opacity = 0.85f);
    void end_glass_layer(ID2D1DeviceContext5* ctx);

    bool is_supported() const { return m_supported; }

private:
    bool m_supported = false;
    ComPtr<ID2D1Effect> m_blur_effect;
    ComPtr<ID2D1Effect> m_shadow_effect;
    ComPtr<ID2D1Effect> m_composite_effect;
};

} // namespace hyperion::ui
