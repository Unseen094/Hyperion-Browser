#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include "hre/css/style_engine.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::css;

class ReplacedEngineTest : public ::testing::Test {
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

TEST_F(ReplacedEngineTest, Replaced_UsesExplicitDimensions) {
    auto node = create_node(L"block", 200, 100);
    node->style.width = CssValue{200, CssUnit::Pixel, L"200px"};
    node->style.height = CssValue{100, CssUnit::Pixel, L"100px"};
    
    engine.layout(node, 300, 300);
    
    // Node should use explicit dimensions
    EXPECT_DOUBLE_EQ(node->content_box.width, 200);
    EXPECT_DOUBLE_EQ(node->content_box.height, 100);
}

TEST_F(ReplacedEngineTest, Replaced_ClampsToAvailableSpace) {
    auto node = create_node(L"block", 400, 400);
    node->style.width = CssValue{400, CssUnit::Pixel, L"400px"};
    node->style.height = CssValue{400, CssUnit::Pixel, L"400px"};
    
    engine.layout(node, 300, 300);
    
    // Node should be clamped to available space
    EXPECT_LE(node->content_box.width, 300);
    EXPECT_LE(node->content_box.height, 300);
}
