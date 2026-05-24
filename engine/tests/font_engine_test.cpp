#include "hre/text/font_engine.hpp"
#include "hre/text/text_shaping.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace hre::text;

// Test fixture for font engine tests
class FontEngineTest : public ::testing::Test {
protected:
    FontEngine& font_engine = FontEngine::instance();
    
    void SetUp() override {
        font_engine.clear_cache();
    }
};

// Test Font loading
TEST_F(FontEngineTest, LoadFont_ValidFamily_ReturnsTrue) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    // Font should load (or gracefully fail on test systems)
    // On Windows with Segoe UI, this should be valid
    // On other systems, it may be invalid but shouldn't crash
    EXPECT_TRUE(font != nullptr);
}

// Test Font metrics
TEST_F(FontEngineTest, FontMetrics_HasValidValues) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        auto metrics = font->get_metrics();
        
        // Metrics should be reasonable
        EXPECT_GT(metrics.ascent, 0);
        EXPECT_GT(metrics.descent, 0);
        EXPECT_GT(metrics.line_height, 0);
    }
}

// Test text shaping
TEST_F(FontEngineTest, ShapeText_EmptyText_ReturnsEmptyGlyphs) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        std::wstring empty = L"";
        auto glyphs = font->shape_text(empty);
        
        EXPECT_TRUE(glyphs.empty());
    }
}

TEST_F(FontEngineTest, ShapeText_SingleChar_ReturnsSingleGlyph) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        std::wstring text = L"A";
        auto glyphs = font->shape_text(text);
        
        EXPECT_EQ(glyphs.size(), 1);
        EXPECT_EQ(glyphs[0].codepoint, L'A');
    }
}

TEST_F(FontEngineTest, ShapeText_MultipleChars_ReturnsCorrectGlyphCount) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        std::wstring text = L"Hello";
        auto glyphs = font->shape_text(text);
        
        EXPECT_EQ(glyphs.size(), text.length());
    }
}

// Test text width measurement
TEST_F(FontEngineTest, GetTextWidth_NonEmptyText_ReturnsPositiveWidth) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        std::wstring text = L"Test";
        float width = font->get_text_width(text);
        
        EXPECT_GT(width, 0);
    }
}

TEST_F(FontEngineTest, GetTextWidth_EmptyText_ReturnsZero) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        std::wstring empty = L"";
        float width = font->get_text_width(empty);
        
        EXPECT_EQ(width, 0);
    }
}

// Test font cache
TEST_F(FontEngineTest, GetFont_SameFamilyAndSize_ReturnsCachedFont) {
    auto font1 = font_engine.get_font(L"Segoe UI", 16.0f);
    auto font2 = font_engine.get_font(L"Segoe UI", 16.0f);
    
    // Should return the same cached instance
    EXPECT_EQ(font1, font2);
}

TEST_F(FontEngineTest, ClearCache_RemovesAllCachedFonts) {
    auto font1 = font_engine.get_font(L"Segoe UI", 16.0f);
    font_engine.clear_cache();
    auto font2 = font_engine.get_font(L"Segoe UI", 16.0f);
    
    // After clearing, should get a new instance
    EXPECT_NE(font1, font2);
}

// Test Text Shaper
class TextShaperTest : public ::testing::Test {
protected:
    TextShaper shaper;
    FontEngine& font_engine = FontEngine::instance();
};

TEST_F(TextShaperTest, Shape_EmptyText_ReturnsEmptyRun) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        ShapedRun run = shaper.shape(L"", font);
        
        EXPECT_TRUE(run.text.empty());
        EXPECT_TRUE(run.glyphs.empty());
        EXPECT_EQ(run.width, 0);
    }
}

TEST_F(TextShaperTest, Shape_SimpleText_ReturnsShapedRun) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        ShapedRun run = shaper.shape(L"Hello", font);
        
        EXPECT_EQ(run.text, L"Hello");
        EXPECT_GT(run.width, 0);
        EXPECT_GT(run.height, 0);
    }
}

TEST_F(TextShaperTest, Shape_LTRText_HasCorrectDirection) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        ShapingOptions options;
        options.direction = L"ltr";
        
        ShapedRun run = shaper.shape(L"Hello", font, options);
        
        EXPECT_EQ(run.options.direction, L"ltr");
    }
}

TEST_F(TextShaperTest, ShapeBidi_DetectsRTL) {
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        // Arabic text
        std::wstring arabic = L"\u0627\u0644\u0645\u0631\u062D\u0628\u0629"; // "Welcome" in Arabic
        auto runs = shaper.shape_bidi(arabic, font);
        
        // Should create at least one run
        EXPECT_GE(runs.size(), 1);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
