#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

#ifdef _WIN32
#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")
#endif

namespace hre::text {

struct GlyphMetrics {
    uint32_t codepoint;
    float x_offset;
    float y_offset;
    float x_advance;
    float y_advance;
    float width;
    float height;
};

struct FontMetrics {
    float ascent;
    float descent;
    float line_height;
    float underline_thickness;
    float underline_position;
};

class Font {
public:
    Font();
    ~Font();

    bool load(const std::wstring& family_name, float size);
    void unload();

    bool is_valid() const { return valid; }
    FontMetrics get_metrics() const;
    std::vector<GlyphMetrics> shape_text(const std::wstring& text);
    float get_text_width(const std::wstring& text);

private:
    bool valid = false;
    float size_px = 16.0f;

#ifdef _WIN32
    IDWriteFactory* dwrite_factory = nullptr;
    IDWriteTextFormat* text_format = nullptr;
    IDWriteFontFace* font_face = nullptr;
#endif
};

class FontEngine {
public:
    static FontEngine& instance();

    std::shared_ptr<Font> get_font(const std::wstring& family, float size);
    void clear_cache();

private:
    FontEngine();
    ~FontEngine();
    std::map<std::wstring, std::shared_ptr<Font>> font_cache;
};

} // namespace hre::text
