#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include "hre/css/style_engine.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::css;

class GridEngineTest : public ::testing::Test {
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

TEST_F(GridEngineTest, Grid_AutoFlowsRowMajor) {
    auto parent = create_node(L"grid", 300, 300);
    parent->style.grid_template_columns = L"100px 100px";
    parent->style.grid_template_rows = L"100px 100px";
    
    auto child1 = create_node(L"block", 50, 50);
    auto child2 = create_node(L"block", 50, 50);
    auto child3 = create_node(L"block", 50, 50);
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    parent->children.push_back(child3);
    
    engine.layout(parent, 300, 300);
    
    // First two children should be in first row
    EXPECT_DOUBLE_EQ(child1->content_box.y, child2->content_box.y);
    // Third child should be in second row
    EXPECT_GT(child3->content_box.y, child1->content_box.y);
}

TEST_F(GridEngineTest, Grid_FractionalUnitsDistributeSpace) {
    auto parent = create_node(L"grid", 300, 100);
    parent->style.grid_template_columns = L"1fr 1fr";
    parent->style.grid_template_rows = L"100px";
    
    auto child1 = create_node(L"block", 50, 50);
    auto child2 = create_node(L"block", 50, 50);
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 300, 100);
    
    // Both columns should have equal width
    EXPECT_DOUBLE_EQ(child1->content_box.width, child2->content_box.width);
}

TEST_F(GridEngineTest, Grid_GapsAreRespected) {
    auto parent = create_node(L"grid", 320, 100);
    parent->style.grid_template_columns = L"100px 100px";
    parent->style.grid_template_rows = L"100px";
    parent->style.column_gap = L"20px";
    
    auto child1 = create_node(L"block", 50, 50);
    auto child2 = create_node(L"block", 50, 50);
    
    parent->children.push_back(child1);
    parent->children.push_back(child2);
    
    engine.layout(parent, 320, 100);
    
    // Second child should be offset by column width + gap
    EXPECT_DOUBLE_EQ(child2->content_box.x, child1->content_box.x + 100 + 20);
}
