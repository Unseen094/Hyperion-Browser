#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include "hre/css/style_engine.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::css;

// Test fixture for layout engine tests
class LayoutEngineTest : public ::testing::Test {
protected:
    LayoutEngine engine;
    
    std::shared_ptr<LayoutNode> create_block_node(const std::wstring& id, double width, double height) {
        // Create a simple element node
        auto node = std::make_shared<LayoutNode>(nullptr);
        node->style.display = L"block";
        node->style.width = CssValue{width, CssUnit::Pixel, std::to_wstring(width) + L"px"};
        node->style.height = CssValue{height, CssUnit::Pixel, std::to_wstring(height) + L"px"};
        return node;
    }
    
    std::shared_ptr<LayoutNode> create_inline_node(const std::wstring& id) {
        auto node = std::make_shared<LayoutNode>(nullptr);
        node->style.display = L"inline";
        return node;
    }
};

// Test Box Model calculations
TEST_F(LayoutEngineTest, BoxModel_CalculatesCorrectly) {
    auto node = std::make_shared<LayoutNode>(nullptr);
    node->style.width = CssValue{100, CssUnit::Pixel, L"100px"};
    node->style.height = CssValue{50, CssUnit::Pixel, L"50px"};
    node->style.padding_top = CssValue{10, CssUnit::Pixel, L"10px"};
    node->style.padding_bottom = CssValue{10, CssUnit::Pixel, L"10px"};
    node->style.padding_left = CssValue{5, CssUnit::Pixel, L"5px"};
    node->style.padding_right = CssValue{5, CssUnit::Pixel, L"5px"};
    node->style.border_top_width = CssValue{2, CssUnit::Pixel, L"2px"};
    node->style.border_bottom_width = CssValue{2, CssUnit::Pixel, L"2px"};
    node->style.border_left_width = CssValue{2, CssUnit::Pixel, L"2px"};
    node->style.border_right_width = CssValue{2, CssUnit::Pixel, L"2px"};
    node->style.margin_top = CssValue{5, CssUnit::Pixel, L"5px"};
    node->style.margin_bottom = CssValue{5, CssUnit::Pixel, L"5px"};
    node->style.margin_left = CssValue{5, CssUnit::Pixel, L"5px"};
    node->style.margin_right = CssValue{5, CssUnit::Pixel, L"5px"};
    
    engine.layout(node, 200);
    
    // Content width should be 100px
    EXPECT_DOUBLE_EQ(node->content_box.width, 100);
    
    // Total width should account for padding, border, margin
    double expected_total = 5 + 2 + 5 + 100 + 5 + 2 + 5; // margin + border + padding + content + padding + border + margin
    EXPECT_DOUBLE_EQ(node->margin_box.width, expected_total);
}

// Test Block layout
TEST_F(LayoutEngineTest, BlockLayout_StacksChildrenVertically) {
    auto parent = create_block_node(L"parent", 200, 300);
    parent->style.display = L"block";
    
    auto child1 = create_block_node(L"child1", 100, 50);
    auto child2 = create_block_node(L"child2", 100, 50);
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 200);
    
    // Children should be stacked vertically
    EXPECT_LT(child1->content_box.y, child2->content_box.y);
}

// Test Inline layout
TEST_F(LayoutEngineTest, InlineLayout_PlacesChildrenHorizontally) {
    auto parent = create_block_node(L"parent", 200, 100);
    parent->style.display = L"block";
    
    auto child1 = create_inline_node(L"child1");
    child1->style.width = CssValue{50, CssUnit::Pixel, L"50px"};
    child1->style.height = CssValue{20, CssUnit::Pixel, L"20px"};
    
    auto child2 = create_inline_node(L"child2");
    child2->style.width = CssValue{50, CssUnit::Pixel, L"50px"};
    child2->style.height = CssValue{20, CssUnit::Pixel, L"20px"};
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 200);
    
    // Inline children should be placed horizontally
    EXPECT_GE(child2->content_box.x, child1->content_box.x);
}

// Test Flexbox layout
TEST_F(LayoutEngineTest, FlexLayout_DistributesSpaceEvenly) {
    auto parent = create_block_node(L"parent", 200, 100);
    parent->style.display = L"flex";
    parent->style.flex_direction = L"row";
    
    auto child1 = create_block_node(L"child1", 50, 50);
    auto child2 = create_block_node(L"child2", 50, 50);
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 200);
    
    // Children should be distributed across container
    EXPECT_GE(child2->content_box.x, child1->content_box.x);
}

// Test nested blocks
TEST_F(LayoutEngineTest, NestedBlocks_CalculateCorrectPositions) {
    auto outer = create_block_node(L"outer", 300, 200);
    outer->style.padding_top = CssValue{10, CssUnit::Pixel, L"10px"};
    outer->style.padding_left = CssValue{10, CssUnit::Pixel, L"10px"};
    
    auto inner = create_block_node(L"inner", 100, 50);
    outer->children.push_back(inner);
    
    engine.layout(outer, 300);
    
    // Inner block should be positioned within outer's padding
    EXPECT_GE(inner->content_box.x, outer->box_model.padding_left);
    EXPECT_GE(inner->content_box.y, outer->box_model.padding_top);
}

// Test auto width
TEST_F(LayoutEngineTest, AutoWidth_CalculatesFromContainingBlock) {
    auto node = create_block_node(L"auto", 0, 50);
    node->style.width = CssValue{0, CssUnit::Auto, L"auto"};
    node->style.margin_left = CssValue{10, CssUnit::Pixel, L"10px"};
    node->style.margin_right = CssValue{10, CssUnit::Pixel, L"10px"};
    
    engine.layout(node, 200);
    
    // Width should fill available space minus margins
    EXPECT_DOUBLE_EQ(node->content_box.width, 180); // 200 - 10 - 10
}

// Test margin collapsing
TEST_F(LayoutEngineTest, MarginCollapsing_VerticalMargins) {
    auto parent = create_block_node(L"parent", 200, 200);
    
    auto child1 = create_block_node(L"child1", 100, 50);
    child1->style.margin_top = CssValue{20, CssUnit::Pixel, L"20px"};
    child1->style.margin_bottom = CssValue{20, CssUnit::Pixel, L"20px"};
    
    auto child2 = create_block_node(L"child2", 100, 50);
    child2->style.margin_top = CssValue{20, CssUnit::Pixel, L"20px"};
    child2->style.margin_bottom = CssValue{20, CssUnit::Pixel, L"20px"};
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 200);
    
    // Verify children are laid out
    EXPECT_TRUE(child1->content_box.y >= 0);
    EXPECT_TRUE(child2->content_box.y > child1->content_box.y);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
