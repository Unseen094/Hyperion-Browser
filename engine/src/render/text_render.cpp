#include <hre/render/text_render.hpp>
#include <cmath>

namespace hre::render {

text_render::text_render() = default;

text_render::~text_render() = default;

void text_render::init(ID2D1DeviceContext5* context, IDWriteFactory* write_factory) {
    m_context = context;
    m_write_factory = write_factory;
}

void text_render::set_antialiasing_mode(bool subpixel) {
    m_subpixel_aa = subpixel;
}

text_run_metrics text_render::measure_text(const std::wstring& text, const std::wstring& font_family, float font_size) {
    text_run_metrics metrics;

    if (text.empty() || !m_write_factory) {
        return metrics;
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_write_factory->CreateTextFormat(
        font_family.empty() ? L"Segoe UI" : font_family.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"en-us",
        &format
    );

    if (FAILED(hr)) {
        return metrics;
    }

    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    hr = m_write_factory->CreateTextLayout(
        text.c_str(),
        static_cast<uint32_t>(text.length()),
        format.Get(),
        4096.0f,
        4096.0f,
        &layout
    );

    if (FAILED(hr)) {
        return metrics;
    }

    DWRITE_TEXT_METRICS metrics_out = {};
    hr = layout->GetMetrics(&metrics_out);

    if (SUCCEEDED(hr)) {
        metrics.width = metrics_out.layoutWidth;
        metrics.height = metrics_out.height;
        metrics.baseline = metrics_out.height * 0.8f;
        metrics.glyph_count = 0;
    }

    return metrics;
}

void text_render::draw_text(ID2D1DeviceContext5* context,
                            const std::wstring& text,
                            float x, float y,
                            const std::wstring& font_family,
                            float font_size,
                            uint32_t color) {
    if (text.empty() || !context || !m_write_factory) {
        return;
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_write_factory->CreateTextFormat(
        font_family.empty() ? L"Segoe UI" : font_family.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"en-us",
        &format
    );

    if (FAILED(hr)) {
        return;
    }

    float a = ((color >> 24) & 0xFF) / 255.0f;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    context->CreateSolidColorBrush(D2D1::ColorF(r, g, b, a), &brush);

    D2D1_POINT_2F point = D2D1::Point2F(x, y);

    context->DrawText(
        text.c_str(),
        static_cast<uint32_t>(text.length()),
        format.Get(),
        D2D1::RectF(x, y, x + 4096, y + 4096),
        brush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
    );
}

void text_render::draw_text_with_shaping(ID2D1DeviceContext5* context,
                                         const std::wstring& text,
                                         float x, float y,
                                         const std::wstring& font_family,
                                         float font_size,
                                         uint32_t color,
                                         const void* shaped_glyphs) {
    draw_text(context, text, x, y, font_family, font_size, color);
}

glyph_cache_entry* text_render::get_cached_glyph(wchar_t ch, const std::wstring& font, float size) {
    glyph_cache_key key{ch, font, size};
    auto it = m_glyph_cache.find(key);
    if (it != m_glyph_cache.end()) {
        return it->second.get();
    }

    if (m_glyph_cache.size() >= MAX_GLYPH_CACHE_SIZE) {
        m_glyph_cache.clear();
    }

    render_glyph_to_cache(ch, font, size);
    it = m_glyph_cache.find(key);
    return it != m_glyph_cache.end() ? it->second.get() : nullptr;
}

void text_render::render_glyph_to_cache(wchar_t ch, const std::wstring& font, float size) {
    if (!m_context || !m_write_factory) return;

    glyph_cache_key key{ch, font, size};

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_write_factory->CreateTextFormat(
        font.empty() ? L"Segoe UI" : font.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"en-us",
        &format
    );

    if (FAILED(hr)) return;

    wchar_t glyph_text[2] = {ch, 0};
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    hr = m_write_factory->CreateTextLayout(
        glyph_text,
        1,
        format.Get(),
        256.0f,
        256.0f,
        &layout
    );

    if (FAILED(hr)) return;

    DWRITE_GLYPH_METRICS gm = {};
    Microsoft::WRL::ComPtr<IDWriteFontFace> font_face;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    hr = m_write_factory->GetSystemFontCollection(&collection);
    if (FAILED(hr)) return;

    auto entry = std::make_unique<glyph_cache_entry>();
    entry->advance = static_cast<float>(gm.advanceWidth);
    entry->is_subpixel = m_subpixel_aa;

    m_glyph_cache[key] = std::move(entry);
}

void text_render::clear_glyph_cache() {
    m_glyph_cache.clear();
}

} // namespace hre::render