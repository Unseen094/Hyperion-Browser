#pragma once

#include "hre/layout/box_model.hpp"
#include "hre/dom/node.hpp"
#include <memory>
#include <vector>
#include <set>

namespace hre::layout {
struct LayoutNode;

struct FloatBox {
    std::shared_ptr<LayoutNode> node;
    double left = 0, top = 0, width = 0, height = 0;
    bool left_float = true;
};

struct BlockContext {
    double current_y = 0;
    double containing_block_width = 0;
    double containing_block_height = 0;
    double available_width = 0;
    std::vector<FloatBox> floats_left;
    std::vector<FloatBox> floats_right;
    double max_float_left = 0;
    double max_float_right = 0;
    bool creates_bfc = false;
};

struct BlockLayoutResult {
    double total_width = 0;
    double total_height = 0;
    double content_width = 0;
    double content_height = 0;
    bool collapsed_margin_top = false;
    bool collapsed_margin_bottom = false;
    double margin_top_collapsed = 0;
    double margin_bottom_collapsed = 0;
};

class BlockLayout {
public:
    BlockLayout() = default;

    BlockLayoutResult layout(
        const std::shared_ptr<LayoutNode>& root,
        double containing_width,
        double containing_height = 0
    );

    void layout_block_flow(const std::shared_ptr<LayoutNode>& parent, BlockContext& context);

    double calculate_auto_width(const std::shared_ptr<LayoutNode>& node, const BoxModel& box_model, double containing_width);
    double calculate_auto_height(const std::shared_ptr<LayoutNode>& node, const BoxModel& box_model);

private:
    double collapse_margins(double margin1, double margin2);
    void position_children(const std::shared_ptr<LayoutNode>& parent, const BoxModel& box_model, BlockContext& context);
    double get_clearance(const std::shared_ptr<LayoutNode>& node, BlockContext& context);
    void handle_float(const std::shared_ptr<LayoutNode>& node, BlockContext& context);
    double get_float_clearance_offset(BlockContext& context, bool clear_left, bool clear_right);
    bool creates_block_formatting_context(const std::shared_ptr<LayoutNode>& node);
    void apply_min_max_constraints(const std::shared_ptr<LayoutNode>& node, double parent_width, double parent_height);
};

} // namespace hre::layout
