#include "hre/layout/layout_engine.hpp"
#include "hre/dom/node.hpp"
#include "hre/css/style_engine.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::css;

class TableEngineTest : public ::testing::Test {
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

TEST_F(TableEngineTest, Table_LaysOutCellsInRows) {
    auto table = create_node(L"table", 300, 200);
    
    auto row = create_node(L"block", 300, 50);
    auto cell1 = create_node(L"block", 100, 50);
    auto cell2 = create_node(L"block", 100, 50);
    
    row->children.push_back(cell1);
    row->children.push_back(cell2);
    table->children.push_back(row);
    
    engine.layout(table, 300, 200);
    
    // Cells should be positioned side by side
    EXPECT_LT(cell1->content_box.x, cell2->content_box.x);
    // Cells should be in the same row
    EXPECT_DOUBLE_EQ(cell1->content_box.y, cell2->content_box.y);
}
