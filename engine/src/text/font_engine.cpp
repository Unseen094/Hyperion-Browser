#include "hre/text/font_engine.hpp"
#include <dwrite.h>
#include <vector>
#include <algorithm>

#ifdef _WIN32

namespace hre::text {

Font::Font() {
    IDWriteFactory* factory = nullptr;
    if (SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&factory)))) {
        dwrite_factory = factory;
    }
}

Font::~Font() {
    if (text_format) text_format->Release();
    if (font_face) font_face->Release();
    if (dwrite_factory) dwrite_factory->Release();
}

bool Font::load(const std::wstring& family_name, float size) {
    if (!dwrite_factory) return false;
    size_px = size;

    HRESULT hr = dwrite_factory->CreateTextFormat(
        family_name.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"en-us",
        &text_format
    );

    if (SUCCEEDED(hr) && text_format) {
        valid = true;
        IDWriteFontCollection* collection = nullptr;
        dwrite_factory->GetSystemFontCollection(&collection, FALSE);
        if (collection) {
            UINT32 index;
            BOOL exists;
            collection->FindFamilyName(family_name.c_str(), &index, &exists);
            if (exists) {
                IDWriteFontFamily* family = nullptr;
                collection->GetFontFamily(index, &family);
                if (family) {
                    IDWriteFont* font = nullptr;
                    family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
                    if (font) {
                        font->CreateFontFace(&font_face);
                        font->Release();
                    }
                    family->Release();
                }
            }
            collection->Release();
        }
        return true;
    }
    return false;
}

void Font::unload() {
    valid = false;
    if (text_format) { text_format->Release(); text_format = nullptr; }
    if (font_face) { font_face->Release(); font_face = nullptr; }
}

FontMetrics Font::get_metrics() const {
    FontMetrics metrics = { 0 };
    if (text_format) {
        metrics.ascent = text_format->GetFontSize() * 0.8f;
        metrics.descent = text_format->GetFontSize() * 0.2f;
        metrics.line_height = text_format->GetFontSize() * 1.2f;
        metrics.underline_thickness = 1.0f;
        metrics.underline_position = -1.0f;
    }
    return metrics;
}

std::vector<GlyphMetrics> Font::shape_text(const std::wstring& text) {
    std::vector<GlyphMetrics> result;
    if (!text_format || text.empty()) return result;

    // Simplified shaping: assume each character advances by font size
    float advance = size_px;
    float current_x = 0.0f;
    
    for (wchar_t ch : text) {
        GlyphMetrics metric;
        metric.codepoint = ch;
        metric.x_offset = current_x;
        metric.y_offset = 0;
        metric.x_advance = advance;
        metric.y_advance = 0;
        metric.width = advance;
        metric.height = size_px;
        result.push_back(metric);
        current_x += advance;
    }
    return result;
}

float Font::get_text_width(const std::wstring& text) {
    if (!text_format || text.empty()) return 0.0f;
    // Simplified: width = char count * font size
    return (float)text.length() * size_px;
}

FontEngine& FontEngine::instance() {
    static FontEngine inst;
    return inst;
}

FontEngine::FontEngine() {}
FontEngine::~FontEngine() {}

std::shared_ptr<Font> FontEngine::get_font(const std::wstring& family, float size) {
    std::wstring key = family + L"_" + std::to_wstring(size);
    auto it = font_cache.find(key);
    if (it != font_cache.end()) return it->second;
    
    auto font = std::make_shared<Font>();
    if (font->load(family, size)) {
        font_cache[key] = font;
        return font;
    }
    return nullptr;
}

void FontEngine::clear_cache() {
    font_cache.clear();
}

} // namespace hre::text

#else
// Non-Windows stub
namespace hre::text {
    Font::Font() {}
    Font::~Font() {}
    bool Font::load(const std::wstring&, float) { return false; }
    void Font::unload() {}
    FontMetrics Font::get_metrics() const { return {}; }
    std::vector<GlyphMetrics> Font::shape_text(const std::wstring&) { return {}; }
    float Font::get_text_width(const std::wstring&) { return 0.0f; }
    FontEngine& FontEngine::instance() { static FontEngine i; return i; }
    std::shared_ptr<Font> FontEngine::get_font(const std::wstring&, float) { return nullptr; }
    void FontEngine::clear_cache() {}
}
#endif
