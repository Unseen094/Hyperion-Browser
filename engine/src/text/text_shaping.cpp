#include "hre/text/text_shaping.hpp"
#include <algorithm>

namespace hre::text {

ShapedRun TextShaper::shape(
    const std::wstring& text,
    const std::shared_ptr<Font>& font,
    const ShapingOptions& options
) {
    ShapedRun run;
    run.text = text;
    run.options = options;
    
    if (!font || !font->is_valid() || text.empty()) {
        return run;
    }
    
    // Get shaped glyphs from font
    run.glyphs = font->shape_text(text);
    
    // Calculate metrics
    run.width = 0;
    run.height = 0;
    
    if (!run.glyphs.empty()) {
        double max_height = 0;
        
        for (const auto& glyph : run.glyphs) {
            run.width = glyph.x_offset + glyph.width;
            if (glyph.height > max_height) {
                max_height = glyph.height;
            }
        }
        
        run.height = max_height;
        
        // Get baseline from font metrics
        auto metrics = font->get_metrics();
        run.baseline = metrics.ascent;
    }
    
    return run;
}

std::vector<ShapedRun> TextShaper::shape_bidi(
    const std::wstring& text,
    const std::shared_ptr<Font>& font,
    const ShapingOptions& options
) {
    std::vector<ShapedRun> runs;
    
    if (text.empty()) return runs;
    
    // Simplified bidirectional algorithm
    // In a full implementation, this would use the Unicode Bidirectional Algorithm
    
    // Detect if text contains RTL characters
    bool has_rtl = false;
    for (wchar_t ch : text) {
        // Basic RTL range check (Arabic, Hebrew)
        if ((ch >= 0x0600 && ch <= 0x06FF) ||  // Arabic
            (ch >= 0x0750 && ch <= 0x077F) ||  // Arabic Supplement
            (ch >= 0x0590 && ch <= 0x05FF)) {  // Hebrew
            has_rtl = true;
            break;
        }
    }
    
    if (has_rtl) {
        // For RTL text, create separate runs
        ShapingOptions rtl_options = options;
        rtl_options.direction = L"rtl";
        
        ShapedRun rtl_run = shape(text, font, rtl_options);
        runs.push_back(std::move(rtl_run));
    } else {
        // LTR text
        ShapedRun ltr_run = shape(text, font, options);
        runs.push_back(std::move(ltr_run));
    }
    
    return runs;
}

GlyphMetrics TextShaper::get_glyph_for_codepoint(
    wchar_t codepoint,
    const std::shared_ptr<Font>& font
) {
    GlyphMetrics metrics;
    metrics.codepoint = codepoint;
    
    if (!font || !font->is_valid()) {
        return metrics;
    }
    
    // Get glyph for single character
    std::wstring text(1, codepoint);
    auto glyphs = font->shape_text(text);
    
    if (!glyphs.empty()) {
        metrics = glyphs[0];
    } else {
        // Fallback metrics
        float size = font->get_metrics().ascent + font->get_metrics().descent;
        metrics.x_advance = size;
        metrics.width = size;
        metrics.height = size;
    }
    
    return metrics;
}

} // namespace hre::text
