#pragma once

#include "hre/css/style_engine.hpp"
#include "hre/layout/box_model.hpp"
#include "hre/layout/block_layout.hpp"
#include "hre/layout/inline_layout.hpp"
#include "hre/layout/flexbox_engine.hpp"
#include "hre/layout/grid_engine.hpp"
#include "hre/layout/positioning_engine.hpp"
#include "hre/layout/table_engine.hpp"
#include "hre/layout/replaced_engine.hpp"
#include "hre/layout/overflow_engine.hpp"
#include "hre/text/font_engine.hpp"
#include <memory>
#include <vector>
#include <string>

namespace hre::layout {


// Layout Box (Rect) - defined in box_model.hpp

// Layout Node (represents a rendered element)
struct LayoutNode {
    const dom::Node* dom_node;
    css::ComputedStyle style;
    LayoutRect content_box;
    LayoutRect padding_box;
    LayoutRect border_box;
    LayoutRect margin_box;
    BoxModel box_model;

    std::vector<std::shared_ptr<LayoutNode>> children;
    std::shared_ptr<LayoutNode> parent;

    LayoutNode(const dom::Node* node) : dom_node(node) {}
};

// Layout Engine - Main entry point
class LayoutEngine {
public:
    LayoutEngine();
    ~LayoutEngine();

    // Build layout tree from DOM
    std::shared_ptr<LayoutNode> build_layout_tree(const dom::Node* root, const css::ComputedStyle& style);

    // Load CSS stylesheets into the internal style engine
    void load_css(const std::wstring& css);
    void load_raw_rules(const std::vector<std::wstring>& rules);

    // Perform layout (reflow)
    void layout(std::shared_ptr<LayoutNode> node, double available_width, double available_height = 0);
    void layout_tree(std::shared_ptr<LayoutNode> node, double available_width, double available_height);

    // Update style for a specific DOM node (e.g. for :hover/:focus state changes)
    void update_style_for_node(std::shared_ptr<LayoutNode> root, const dom::Element* target,
                               bool is_hovered, bool is_focused);

    // Get layout dimensions
    LayoutRect get_bounds(const std::shared_ptr<LayoutNode>& node) const;

private:
    css::StyleEngine style_engine;
    BlockLayout block_layout;
    InlineLayout inline_layout;
    FlexboxEngine flexbox_engine;
    GridEngine grid_engine;
    PositioningEngine positioning_engine;
    TableEngine table_engine;
    ReplacedEngine replaced_engine;
    OverflowEngine overflow_engine;

    // Box Model Calculations
    void calculate_box_model(std::shared_ptr<LayoutNode> node, double parent_width);

    // Layout Algorithms
    void layout_block(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_inline(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_flex(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_grid(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_positioned(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_table(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_replaced(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_overflow(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    
    // Text content layout
    void layout_text_content(std::shared_ptr<LayoutNode> node, double available_width);
};

} // namespace hre::layout
