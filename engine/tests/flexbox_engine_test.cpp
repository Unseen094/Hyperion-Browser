#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include "hre/css/style_engine.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::css;

class FlexboxEngineTest : public ::testing::Test {
protected:
    LayoutEngine engine;
    
    std::shared_ptr<LayoutNode> create_node(const std::wstring& display, double width, double height) {
        auto node = std::make_shared<LayoutNode>(nullptr);
        node->style.display = display;
        node->style.width = CssValue{width, CssUnit::Pixel, std::to_wstring(width) + L"px"};
        node->style.height = CssValue{height, CssUnit::Pixel, std::to_wstring(height) + L"px"};
        return node;
    }
};

TEST_F(FlexboxEngineTest, FlexRow_DistributesChildrenHorizontally) {
    auto parent = create_node(L"flex", 300, 100);
    parent->style.flex_direction = L"row";
    
    auto child1 = create_node(L"block", 50, 50);
    auto child2 = create_node(L"block", 50, 50);
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 300, 100);
    
    EXPECT_GE(child2->content_box.x, child1->content_box.x);
}

TEST_F(FlexboxEngineTest, FlexGrow_ExpandsChildrenToFillSpace) {
    auto parent = create_node(L"flex", 300, 100);
    parent->style.flex_direction = L"row";
    
    auto child1 = create_node(L"block", 50, 50);
    child1->style.flex_grow = 1.0;
    
    auto child2 = create_node(L"block", 50, 50);
    child2->style.flex_grow = 1.0;
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 300, 100);
    
    // Both children should have grown to fill available space
    EXPECT_DOUBLE_EQ(child1->content_box.width, child2->content_box.width);
}

TEST_F(FlexboxEngineTest, JustifyContent_CentersChildren) {
    auto parent = create_node(L"flex", 300, 100);
    parent->style.flex_direction = L"row";
    parent->style.justify_content = L"center";
    
    auto child1 = create_node(L"block", 50, 50);
    auto child2 = create_node(L"block", 50, 50);
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 300, 100);
    
    // Children should be centered
    EXPECT_GT(child1->content_box.x, 0);
}

TEST_F(FlexboxEngineTest, AlignItems_StretchesChildrenVertically) {
    auto parent = create_node(L"flex", 300, 100);
    parent->style.flex_direction = L"row";
    parent->style.align_items = L"stretch";
    
    auto child1 = create_node(L"block", 50, 50);
    
    parent->children.push_back(child1);
    
    engine.layout(parent, 300, 100);
    
    // Child should be stretched to container height
    EXPECT_DOUBLE_EQ(child1->content_box.height, 100);
}
