#include "hre/render/text_render.hpp"
#include <gtest/gtest.h>

using namespace hre::render;

class TextRenderTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(TextRenderTest, DefaultConstructor_InitializesCorrectly) {
    text_render renderer;
    EXPECT_FALSE(renderer.get_antialiasing_mode());
}

TEST_F(TextRenderTest, SetAntialiasingMode_UpdatesSubpixelFlag) {
    text_render renderer;
    renderer.set_antialiasing_mode(true);
    EXPECT_TRUE(renderer.get_antialiasing_mode());

    renderer.set_antialiasing_mode(false);
    EXPECT_FALSE(renderer.get_antialiasing_mode());
}

TEST_F(TextRenderTest, GlyphCache_StartsEmpty) {
    text_render renderer;
    EXPECT_EQ(renderer.glyph_cache_size(), 0u);
}

TEST_F(TextRenderTest, ClearGlyphCache_EmptiesCache) {
    text_render renderer;
    renderer.clear_glyph_cache();
    EXPECT_EQ(renderer.glyph_cache_size(), 0u);
}

TEST_F(TextRenderTest, MeasureText_EmptyString_ReturnsZeroMetrics) {
    text_render renderer;
    auto metrics = renderer.measure_text(L"", L"Segoe UI", 14.0f);

    EXPECT_EQ(metrics.width, 0.0f);
    EXPECT_EQ(metrics.height, 0.0f);
    EXPECT_EQ(metrics.glyph_count, 0u);
}

TEST_F(TextRenderTest, MeasureText_SingleCharacter_ReturnsValidMetrics) {
    text_render renderer;
    auto metrics = renderer.measure_text(L"A", L"Segoe UI", 14.0f);

    EXPECT_GT(metrics.width, 0.0f);
    EXPECT_GT(metrics.height, 0.0f);
}