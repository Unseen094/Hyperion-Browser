#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <dwrite.h>
#include <d2d1_3.h>

namespace hre::render {

struct glyph_cache_entry {
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    float advance = 0.0f;
    bool is_subpixel = false;
};

struct text_run_metrics {
    float width = 0.0f;
    float height = 0.0f;
    float baseline = 0.0f;
    size_t glyph_count = 0;
};

class text_render {
public:
    text_render();
    ~text_render();

    void init(ID2D1DeviceContext5* context, IDWriteFactory* write_factory);

    void set_antialiasing_mode(bool subpixel);
    bool get_antialiasing_mode() const { return m_subpixel_aa; }

    void clear_glyph_cache();
    size_t glyph_cache_size() const { return m_glyph_cache.size(); }

    text_run_metrics measure_text(const std::wstring& text, const std::wstring& font_family, float font_size);

    void draw_text(ID2D1DeviceContext5* context,
                   const std::wstring& text,
                   float x, float y,
                   const std::wstring& font_family,
                   float font_size,
                   uint32_t color);

    void draw_text_with_shaping(ID2D1DeviceContext5* context,
                                const std::wstring& text,
                                float x, float y,
                                const std::wstring& font_family,
                                float font_size,
                                uint32_t color,
                                const void* shaped_glyphs = nullptr);

private:
    glyph_cache_entry* get_cached_glyph(wchar_t ch, const std::wstring& font, float size);
    void render_glyph_to_cache(wchar_t ch, const std::wstring& font, float size);

    ID2D1DeviceContext5* m_context = nullptr;
    IDWriteFactory* m_write_factory = nullptr;
    bool m_subpixel_aa = true;

    struct glyph_cache_key {
        wchar_t ch;
        std::wstring font;
        float size;
        bool operator<(const glyph_cache_key& other) const {
            if (ch != other.ch) return ch < other.ch;
            if (font != other.font) return font < other.font;
            return size < other.size;
        }
    };

    std::map<glyph_cache_key, std::unique_ptr<glyph_cache_entry>> m_glyph_cache;
    static constexpr size_t MAX_GLYPH_CACHE_SIZE = 8192;
};

} // namespace hre::render