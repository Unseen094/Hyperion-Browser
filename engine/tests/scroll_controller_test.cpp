#include "hre/render/scroll_controller.hpp"
#include <gtest/gtest.h>

using namespace hre::render;

class ScrollControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ScrollControllerTest, DefaultConstructor_InitializesToZero) {
    scroll_controller controller;
    auto pos = controller.get_position();

    EXPECT_EQ(pos.x, 0.0f);
    EXPECT_EQ(pos.y, 0.0f);
    EXPECT_EQ(pos.target_x, 0.0f);
    EXPECT_EQ(pos.target_y, 0.0f);
    EXPECT_FALSE(controller.is_animating());
}

TEST_F(ScrollControllerTest, SetScrollPosition_UpdatesPosition) {
    scroll_controller controller;
    controller.set_scroll_position(100, 200);

    auto pos = controller.get_position();
    EXPECT_EQ(pos.x, 100.0f);
    EXPECT_EQ(pos.y, 200.0f);
}

TEST_F(ScrollControllerTest, ScrollTo_WithZeroDuration_SetsPositionImmediately) {
    scroll_controller controller;
    controller.set_max_scroll(500, 1000);
    controller.scroll_to(100, 200, 0);

    auto pos = controller.get_position();
    EXPECT_EQ(pos.x, 100.0f);
    EXPECT_EQ(pos.y, 200.0f);
    EXPECT_FALSE(controller.is_animating());
}

TEST_F(ScrollControllerTest, ScrollTo_WithDuration_SetsTargetAndStartsAnimating) {
    scroll_controller controller;
    controller.scroll_to(100, 200, 300);

    auto pos = controller.get_position();
    EXPECT_EQ(pos.target_x, 100.0f);
    EXPECT_EQ(pos.target_y, 200.0f);
    EXPECT_TRUE(controller.is_animating());
}

TEST_F(ScrollControllerTest, ScrollBy_AdjustsCurrentPosition) {
    scroll_controller controller;
    controller.set_scroll_position(100, 100);
    controller.scroll_by(50, 75);

    auto pos = controller.get_position();
    EXPECT_EQ(pos.x, 150.0f);
    EXPECT_EQ(pos.y, 175.0f);
}

TEST_F(ScrollControllerTest, SetMaxScroll_SetsBounds) {
    scroll_controller controller;
    controller.set_max_scroll(500, 1000);

    EXPECT_EQ(controller.max_scroll_x(), 500.0f);
    EXPECT_EQ(controller.max_scroll_y(), 1000.0f);
}

TEST_F(ScrollControllerTest, SetScrollPosition_BeyondMax_Clamps) {
    scroll_controller controller;
    controller.set_max_scroll(200, 400);
    controller.set_scroll_position(500, 1000);

    auto pos = controller.get_position();
    EXPECT_EQ(pos.x, 200.0f);
    EXPECT_EQ(pos.y, 400.0f);
}

TEST_F(ScrollControllerTest, Stop_StopsAnimation) {
    scroll_controller controller;
    controller.scroll_to(100, 200, 300);
    EXPECT_TRUE(controller.is_animating());

    controller.stop();
    EXPECT_FALSE(controller.is_animating());
}

TEST_F(ScrollControllerTest, Overscroll_Disabled_ClampsToBounds) {
    scroll_controller controller;
    controller.set_overscroll_enabled(false);
    controller.set_max_scroll(100, 100);
    controller.scroll_by(-50, -50);

    auto pos = controller.get_position();
    EXPECT_EQ(pos.x, 0.0f);
    EXPECT_EQ(pos.y, 0.0f);
}

TEST_F(ScrollControllerTest, Update_NoAnimation_DoesNothing) {
    scroll_controller controller;
    controller.set_scroll_position(50, 50);
    controller.update(16);

    auto pos = controller.get_position();
    EXPECT_EQ(pos.x, 50.0f);
    EXPECT_EQ(pos.y, 50.0f);
    EXPECT_FALSE(controller.is_animating());
}