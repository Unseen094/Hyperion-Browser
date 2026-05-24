#pragma once

#include <dwrite_3.h>
#include <wrl/client.h>

namespace hyperion::ui {

using Microsoft::WRL::ComPtr;

// Create text format with backward compatibility for newer DWrite factories
inline HRESULT create_text_format(
    IDWriteFactory7* factory,
    const wchar_t* font_family,
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STYLE style,
    DWRITE_FONT_STRETCH stretch,
    float size,
    const wchar_t* locale,
    IDWriteTextFormat** out_format
) {
    // Try old-school DWriteFactory first
    ComPtr<IDWriteFactory> base_factory;
    if (SUCCEEDED(factory->QueryInterface(__uuidof(IDWriteFactory), (void**)&base_factory))) {
        return base_factory->CreateTextFormat(font_family, nullptr, weight, style, stretch, size, locale, out_format);
    }
    // Fallback: use new axis-value overload with no custom axes
    ComPtr<IDWriteTextFormat3> fmt3;
    HRESULT hr = factory->CreateTextFormat(font_family, nullptr, nullptr, 0, size, locale, &fmt3);
    if (SUCCEEDED(hr) && fmt3) {
        *out_format = fmt3.Detach();
    }
    return hr;
}

} // namespace hyperion::ui
