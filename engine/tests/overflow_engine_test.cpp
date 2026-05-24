#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include "hre/css/style_engine.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::css;

class OverflowEngineTest : public ::testing::Test {
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

TEST_F(OverflowEngineTest, Overflow_ClampsToAvailableSpace) {
    auto node = create_node(L"block", 400, 400);
    node->style.width = CssValue{400, CssUnit::Pixel, L"400px"};
    node->style.height = CssValue{400, CssUnit::Pixel, L"400px"};
    
    engine.layout(node, 300, 300);
    
    // Content should be clamped to available space
    EXPECT_LE(node->content_box.width, 300);
    EXPECT_LE(node->content_box.height, 300);
}

TEST_F(OverflowEngineTest, Overflow_FitsWithinContainer) {
    auto parent = create_node(L"block", 300, 300);
    auto child = create_node(L"block", 350, 350);
    
    parent->children.push_back(child);
    
    engine.layout(parent, 300, 300);
    
    // Child should fit within parent
    EXPECT_LE(child->content_box.width, parent->content_box.width);
    EXPECT_LE(child->content_box.height, parent->content_box.height);
}
