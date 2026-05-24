#pragma once

#include "hre/text/font_engine.hpp"
#include <string>
#include <vector>
#include <map>

namespace hre::text {

// Text shaping options
struct ShapingOptions {
    std::wstring direction = L"ltr"; // ltr, rtl
    std::wstring script = L"latin";
    std::wstring language = L"en";
    bool enable_ligatures = true;
    bool enable_kerning = true;
};

// Shaped run (contiguous run of text with same formatting)
struct ShapedRun {
    std::wstring text;
    std::vector<GlyphMetrics> glyphs;
    double width = 0;
    double height = 0;
    double baseline = 0;
    ShapingOptions options;
};

// Text shaper class
class TextShaper {
public:
    TextShaper() = default;
    ~TextShaper() = default;
    
    // Shape text into glyphs
    ShapedRun shape(
        const std::wstring& text,
        const std::shared_ptr<Font>& font,
        const ShapingOptions& options = ShapingOptions()
    );
    
    // Shape bidirectional text
    std::vector<ShapedRun> shape_bidi(
        const std::wstring& text,
        const std::shared_ptr<Font>& font,
        const ShapingOptions& options = ShapingOptions()
    );
    
    // Get character bounds
    GlyphMetrics get_glyph_for_codepoint(
        wchar_t codepoint,
        const std::shared_ptr<Font>& font
    );

    // Set text direction
    void set_direction(const std::wstring& dir) { m_direction = dir; }

private:
    std::wstring m_direction = L"ltr";
};

} // namespace hre::text