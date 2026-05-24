#include "hre/render/display_list.hpp"
#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include <gtest/gtest.h>
#include <memory>

using namespace hre::render;
using namespace hre::layout;

class DisplayListTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(DisplayListTest, EmptyDisplayList_HasZeroItems) {
    display_list list;
    EXPECT_EQ(list.item_count(), 0u);
}

TEST_F(DisplayListTest, AddSolidRect_IncreasesItemCount) {
    display_list list;
    list.add_solid_rect(10, 20, 100, 50, 0xFFFF0000);
    EXPECT_EQ(list.item_count(), 1u);
}

TEST_F(DisplayListTest, AddRoundedRect_SetsCorrectProperties) {
    display_list list;
    list.add_rounded_rect(0, 0, 100, 100, 8.0f, 0xFF00FF00);

    ASSERT_EQ(list.item_count(), 1u);
    const auto& item = list.items()[0];
    EXPECT_EQ(item.kind, display_item::dl_item_type::ROUNDED_RECT);
    EXPECT_EQ(item.border_radius, 8.0f);
    EXPECT_EQ(item.bg_color, 0xFF00FF00);
}

TEST_F(DisplayListTest, AddTextRun_SetsCorrectProperties) {
    display_list list;
    list.add_text_run(10, 10, L"Hello World", L"Arial", 14.0f, 0xFF0000FF);

    ASSERT_EQ(list.item_count(), 1u);
    const auto& item = list.items()[0];
    EXPECT_EQ(item.kind, display_item::dl_item_type::TEXT_RUN);
    EXPECT_EQ(item.text_content, L"Hello World");
    EXPECT_EQ(item.font_family, L"Arial");
    EXPECT_EQ(item.font_size, 14.0f);
    EXPECT_EQ(item.text_color, 0xFF0000FF);
}

TEST_F(DisplayListTest, AddBoxShadow_SetsCorrectProperties) {
    display_list list;
    list.add_box_shadow(0, 0, 100, 50, 5, 5, 10, 0x80000000);

    ASSERT_EQ(list.item_count(), 1u);
    const auto& item = list.items()[0];
    EXPECT_EQ(item.kind, display_item::dl_item_type::SHADOW_ITEM);
    EXPECT_EQ(item.shadow_offset_x, 5);
    EXPECT_EQ(item.shadow_offset_y, 5);
    EXPECT_EQ(item.shadow_blur, 10);
}

TEST_F(DisplayListTest, Clear_RemovesAllItems) {
    display_list list;
    list.add_solid_rect(0, 0, 100, 100, 0xFFFF0000);
    list.add_solid_rect(10, 10, 50, 50, 0xFF00FF00);

    EXPECT_EQ(list.item_count(), 2u);
    list.clear();
    EXPECT_EQ(list.item_count(), 0u);
}

TEST_F(DisplayListTest, ClipStack_TracksNesting) {
    display_list list;
    EXPECT_TRUE(list.items().empty());

    list.push_clip(1);
    list.add_solid_rect(0, 0, 100, 100, 0xFFFF0000);

    ASSERT_EQ(list.item_count(), 1u);
    EXPECT_TRUE(list.items()[0].is_clipped);
    EXPECT_EQ(list.items()[0].clip_id, 1);

    list.pop_clip();
    list.add_solid_rect(0, 0, 50, 50, 0xFF00FF00);

    ASSERT_EQ(list.item_count(), 2u);
    EXPECT_FALSE(list.items()[1].is_clipped);
}

TEST_F(DisplayListTest, AddMultipleItems_preservesOrder) {
    display_list list;
    list.add_solid_rect(0, 0, 100, 100, 0xFFFF0000);
    list.add_rounded_rect(10, 10, 50, 50, 4.0f, 0xFF00FF00);
    list.add_text_run(5, 5, L"Test", L"Segoe UI", 12.0f, 0xFF0000FF);

    ASSERT_EQ(list.item_count(), 3u);
    EXPECT_EQ(list.items()[0].kind, display_item::dl_item_type::SOLID_RECT);
    EXPECT_EQ(list.items()[1].kind, display_item::dl_item_type::ROUNDED_RECT);
    EXPECT_EQ(list.items()[2].kind, display_item::dl_item_type::TEXT_RUN);
}