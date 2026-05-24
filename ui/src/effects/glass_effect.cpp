#include <hyperion/ui/effects/glass_effect.hpp>
#include <hyperion/platform/logging.hpp>

namespace hyperion::ui {

glass_effect::glass_effect() = default;
glass_effect::~glass_effect() = default;

bool glass_effect::initialize(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return false;

    HRESULT hr;
    hr = ctx->CreateEffect(CLSID_D2D1GaussianBlur, &m_blur_effect);
    if (FAILED(hr)) {
        HYPERION_LOG_WARN("D2D Gaussian blur effect not supported, using fallback");
        m_supported = false;
        return false;
    }

    hr = ctx->CreateEffect(CLSID_D2D1Shadow, &m_shadow_effect);
    if (SUCCEEDED(hr)) {
        m_supported = true;
        HYPERION_LOG_INFO("D2D 1.3 effects pipeline initialized");
    }

    return m_supported;
}

void glass_effect::apply_backdrop_blur(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect, float blur_radius) {
    if (!m_supported || !m_blur_effect) return;
    ComPtr<ID2D1Image> target;
    ctx->GetTarget(&target);
    m_blur_effect->SetInput(0, target.Get());
    m_blur_effect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, blur_radius);
    m_blur_effect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    ctx->DrawImage(m_blur_effect.Get(), D2D1::Point2F(rect.left, rect.top),
                   D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom));
}

void glass_effect::begin_glass_layer(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect, float opacity) {
    if (!m_supported) return;
    ComPtr<ID2D1Image> target;
    ctx->GetTarget(&target);
    ComPtr<ID2D1Bitmap1> layer_bitmap;
    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ctx->CreateBitmap(D2D1::SizeU((UINT32)(rect.right - rect.left), (UINT32)(rect.bottom - rect.top)),
                      nullptr, 0, &props, &layer_bitmap);
    ctx->SetTarget(layer_bitmap.Get());
}

void glass_effect::end_glass_layer(ID2D1DeviceContext5* ctx) {
    if (!m_supported) return;
    ctx->SetTarget(nullptr);
}

void glass_effect::apply_drop_shadow(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect, float radius) {
    if (!m_supported || !m_shadow_effect) return;
    ComPtr<ID2D1Image> target;
    ctx->GetTarget(&target);
    m_shadow_effect->SetInput(0, target.Get());
    m_shadow_effect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, radius);
    m_shadow_effect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(0, 0, 0, 0.3f));
    ctx->DrawImage(m_shadow_effect.Get(), D2D1::Point2F(rect.left + 2, rect.top + 2));
}

} // namespace hyperion::ui
