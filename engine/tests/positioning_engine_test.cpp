#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include "hre/css/style_engine.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::css;

class PositioningEngineTest : public ::testing::Test {
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

TEST_F(PositioningEngineTest, Absolute_PositionsRelativeToAncestor) {
    auto parent = create_node(L"block", 300, 300);
    parent->style.position = L"relative";
    
    auto child = create_node(L"block", 50, 50);
    child->style.position = L"absolute";
    child->style.margin_left = CssValue{20, CssUnit::Pixel, L"20px"};
    child->style.margin_top = CssValue{30, CssUnit::Pixel, L"30px"};
    
    parent->children.push_back(child);
    
    engine.layout(parent, 300, 300);
    
    // Child should be positioned at ancestor origin + offsets
    EXPECT_DOUBLE_EQ(child->content_box.x, 20);
    EXPECT_DOUBLE_EQ(child->content_box.y, 30);
}

TEST_F(PositioningEngineTest, Relative_AppliesOffsetsFromNormalFlow) {
    auto parent = create_node(L"block", 300, 300);
    
    auto child = create_node(L"block", 50, 50);
    child->style.position = L"relative";
    child->style.margin_left = CssValue{10, CssUnit::Pixel, L"10px"};
    child->style.margin_top = CssValue{20, CssUnit::Pixel, L"20px"};
    
    parent->children.push_back(child);
    
    engine.layout(parent, 300, 300);
    
    // Child should be offset from its normal position
    EXPECT_GE(child->content_box.x, 10);
    EXPECT_GE(child->content_box.y, 20);
}
