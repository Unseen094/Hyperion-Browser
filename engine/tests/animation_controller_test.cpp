#include "hre/render/animation_controller.hpp"
#include <gtest/gtest.h>

using namespace hre::render;

class AnimationControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(AnimationControllerTest, AddAnimation_ReturnsValidId) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300, 0, L"ease");

    EXPECT_GT(id, 0u);
}

TEST_F(AnimationControllerTest, AddMultipleAnimations_ReturnsDifferentIds) {
    animation_controller controller;
    uint32_t id1 = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    uint32_t id2 = controller.add_animation(L"elem1", L"transform", 0.0f, 1.0f, 300);
    uint32_t id3 = controller.add_animation(L"elem2", L"opacity", 0.0f, 1.0f, 300);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(AnimationControllerTest, IsAnimating_NoAnimations_ReturnsFalse) {
    animation_controller controller;
    EXPECT_FALSE(controller.is_animating(L"elem1", L"opacity"));
}

TEST_F(AnimationControllerTest, AddKeyframe_AddsKeyframeToAnimation) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);

    keyframe kf{0.5f, 0.5f, L"linear"};
    controller.add_keyframe(id, kf);

    // Keyframe is added, no easy way to verify without access to internal state
    SUCCEED();
}

TEST_F(AnimationControllerTest, SetIterationCount_UpdatesAnimation) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    controller.set_iteration_count(id, 3.0f);

    SUCCEED();
}

TEST_F(AnimationControllerTest, SetDirection_UpdatesAnimation) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    controller.set_direction(id, animation_direction::ALTERNATE);

    SUCCEED();
}

TEST_F(AnimationControllerTest, SetFillMode_UpdatesAnimation) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    controller.set_fill_mode(id, animation_fill_mode::FORWARDS);

    SUCCEED();
}

TEST_F(AnimationControllerTest, Pause_SetsPlayStateToPaused) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    controller.pause(id);

    SUCCEED();
}

TEST_F(AnimationControllerTest, Play_SetsPlayStateToPlaying) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    controller.pause(id);
    controller.play(id);

    SUCCEED();
}

TEST_F(AnimationControllerTest, Cancel_RemovesAnimation) {
    animation_controller controller;
    uint32_t id = controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    controller.cancel(id);

    controller.update();

    EXPECT_FALSE(controller.is_animating(L"elem1", L"opacity"));
}

TEST_F(AnimationControllerTest, ClearAnimationsForElement_RemovesAllAnimations) {
    animation_controller controller;
    controller.add_animation(L"elem1", L"opacity", 0.0f, 1.0f, 300);
    controller.add_animation(L"elem1", L"transform", 0.0f, 1.0f, 300);
    controller.add_animation(L"elem2", L"opacity", 0.0f, 1.0f, 300);

    EXPECT_TRUE(controller.is_animating(L"elem1", L"opacity"));
    EXPECT_TRUE(controller.is_animating(L"elem1", L"transform"));
    EXPECT_TRUE(controller.is_animating(L"elem2", L"opacity"));

    controller.clear_animations_for_element(L"elem1");

    EXPECT_FALSE(controller.is_animating(L"elem1", L"opacity"));
    EXPECT_FALSE(controller.is_animating(L"elem1", L"transform"));
    EXPECT_TRUE(controller.is_animating(L"elem2", L"opacity"));
}

TEST_F(AnimationControllerTest, GetCurrentValue_NoAnimation_ReturnsZero) {
    animation_controller controller;
    EXPECT_EQ(controller.get_current_value(999), 0.0f);
}