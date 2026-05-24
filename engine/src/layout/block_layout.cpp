#define NOMINMAX
#include "hre/layout/block_layout.hpp"
#include "hre/layout/layout_engine.hpp"
#include <algorithm>
#include <cmath>

namespace hre::layout {

double BlockLayout::collapse_margins(double margin1, double margin2) {
    if (margin1 >= 0 && margin2 >= 0) return std::max(margin1, margin2);
    if (margin1 < 0 && margin2 < 0) return std::min(margin1, margin2);
    return margin1 + margin2;
}

bool BlockLayout::creates_block_formatting_context(const std::shared_ptr<LayoutNode>& node) {
    const auto& s = node->style;
    return s.display == L"flow-root" || s.display == L"flex" || s.display == L"grid" ||
            s.position == L"absolute" || s.position == L"fixed" ||
            s.overflow != L"visible" ||
            s.display == L"inline-block" || s.display == L"table-cell" ||
           s.display == L"table-caption" || s.contain.find(L"layout") != std::wstring::npos;
}

double BlockLayout::get_clearance(const std::shared_ptr<LayoutNode>& node, BlockContext& context) {
    return 0;
}

double BlockLayout::get_float_clearance_offset(BlockContext& context, bool clear_left, bool clear_right) {
    double max_y = context.current_y;
    if (clear_left) {
        for (const auto& fb : context.floats_left) {
            max_y = std::max(max_y, fb.top + fb.height);
        }
        context.floats_left.clear();
        context.max_float_left = 0;
    }
    if (clear_right) {
        for (const auto& fb : context.floats_right) {
            max_y = std::max(max_y, fb.top + fb.height);
        }
        context.floats_right.clear();
        context.max_float_right = 0;
    }
    return std::max(0.0, max_y - context.current_y);
}

void BlockLayout::handle_float(const std::shared_ptr<LayoutNode>& node, BlockContext& context) {
    FloatBox fb;
    fb.node = node;
    fb.left_float = true;
    fb.width = node->content_box.width;
    fb.height = node->content_box.height;

    double container_width = context.containing_block_width;
    double left_edge = context.max_float_left;
    double right_edge = container_width - context.max_float_right;

    if (fb.left_float) {
        fb.left = left_edge;
        fb.top = context.current_y;
        context.max_float_left = left_edge + fb.width;
        context.floats_left.push_back(fb);
    } else {
        fb.left = right_edge - fb.width;
        fb.top = context.current_y;
        context.max_float_right = container_width - (right_edge - fb.width);
        context.floats_right.push_back(fb);
    }
}

void BlockLayout::apply_min_max_constraints(const std::shared_ptr<LayoutNode>& node, double parent_width, double parent_height) {
    auto& s = node->style;
    double w = node->content_box.width;
    double h = node->content_box.height;

    if (s.min_width.unit != css::CssUnit::Auto) {
        double min_w = s.min_width.to_pixels(parent_width);
        w = std::max(w, min_w);
    }
    if (s.max_width.unit != css::CssUnit::None) {
        double max_w = s.max_width.to_pixels(parent_width);
        w = std::min(w, max_w);
    }
    if (s.min_height.unit != css::CssUnit::Auto) {
        double min_h = s.min_height.to_pixels(parent_height);
        h = std::max(h, min_h);
    }
    if (s.max_height.unit != css::CssUnit::None) {
        double max_h = s.max_height.to_pixels(parent_height);
        h = std::min(h, max_h);
    }

    node->content_box.width = w;
    node->content_box.height = h;
}

double BlockLayout::calculate_auto_width(const std::shared_ptr<LayoutNode>& node, const BoxModel& box_model, double containing_width) {
    double available = containing_width - box_model.get_border_width() - box_model.get_padding_width();
    double margin_width = box_model.margin_left + box_model.margin_right;
    available -= margin_width;

    return std::max(0.0, available);
}

double BlockLayout::calculate_auto_height(const std::shared_ptr<LayoutNode>& node, const BoxModel& box_model) {
    return 0;
}

void BlockLayout::position_children(const std::shared_ptr<LayoutNode>& parent, const BoxModel& box_model, BlockContext& context) {
    double current_y = context.current_y;
    double content_left = box_model.padding_left + box_model.border_left;
    double content_top = box_model.padding_top + box_model.border_top;
    double containing_width = context.containing_block_width;
    double available_content_width = containing_width - box_model.padding_left - box_model.padding_right - box_model.border_left - box_model.border_right;

    for (auto& child : parent->children) {
        if (!child) continue;

        BoxModel child_box = calculate_box_model(child->style, containing_width, context.containing_block_height);

        // Min/max constraints
        apply_min_max_constraints(child, containing_width, context.containing_block_height);

        // Clearance
        double clearance = get_clearance(child, context);
        current_y += clearance;

        // Margin collapsing with previous sibling
        double prev_margin_bottom = 0;
        if (current_y > 0) {
            double child_margin_top = child_box.margin_top;
            current_y = collapse_margins(current_y, child_margin_top);
        }

        // Float handling
        if (!creates_block_formatting_context(child)) {
            handle_float(child, context);
            BoxModel child_box_float = calculate_box_model(child->style, available_content_width, context.containing_block_height);
            if (child->style.width.unit == css::CssUnit::Auto) {
                child->content_box.width = calculate_auto_width(child, child_box_float, available_content_width);
            } else {
                child->content_box.width = child_box_float.content_width;
            }
            apply_min_max_constraints(child, containing_width, context.containing_block_height);
            continue;
        }

        // Set position
        double x_offset = content_left + box_model.margin_left;
        if (context.max_float_left > 0 || context.max_float_right > 0) {
            x_offset += context.max_float_left;
        }
        child->content_box.x = x_offset;
        child->content_box.y = current_y;

        // Calculate width
        if (child->style.width.unit == css::CssUnit::Auto) {
            child->content_box.width = calculate_auto_width(child, child_box, available_content_width);
        } else {
            child->content_box.width = child_box.content_width;
        }
        apply_min_max_constraints(child, containing_width, context.containing_block_height);

        // Layout child recursively based on display
        const std::wstring& display = child->style.display;
        const std::wstring& pos = child->style.position;

        if (pos == L"absolute" || pos == L"fixed") {
            // Positioned elements are taken out of flow
            continue;
        }

        if (display == L"block" || display == L"flow-root" || creates_block_formatting_context(child)) {
            BlockContext child_ctx;
            child_ctx.containing_block_width = child->content_box.width;
            child_ctx.containing_block_height = 0;
            child_ctx.current_y = 0;
            child_ctx.creates_bfc = true;
            layout_block_flow(child, child_ctx);
            child->content_box.height = child_ctx.current_y;
        } else if (display == L"flex" || display == L"inline-flex") {
            FlexboxEngine flex;
            flex.layout_flex_container(child, child->content_box.width, context.containing_block_height);
        } else if (display == L"grid" || display == L"inline-grid") {
            GridEngine grid;
            grid.layout_grid_container(child, child->content_box.width, context.containing_block_height);
        } else if (display == L"inline" || display == L"inline-block") {
            InlineLayout inline_layout;
            double ih = 0;
            inline_layout.layout_inline_content(child, child_box, child->content_box.width, ih);
            child->content_box.height = ih;
        } else if (display.find(L"table") == 0) {
            TableEngine tbl;
            tbl.layout_table(child, child->content_box.width, context.containing_block_height);
        }

        // Advance Y
        double child_total_height = child_box.margin_top + child_box.border_top +
                                    child->content_box.height + child_box.padding_top + child_box.padding_bottom +
                                    child_box.border_bottom + child_box.margin_bottom;
        current_y += child_total_height;
    }

    context.current_y = current_y;
}

BlockLayoutResult BlockLayout::layout(const std::shared_ptr<LayoutNode>& root, double containing_width, double containing_height) {
    BlockLayoutResult result;
    if (!root) return result;

    BoxModel box_model = calculate_box_model(root->style, containing_width, containing_height);
    if (root->style.width.unit == css::CssUnit::Auto) {
        box_model.content_width = calculate_auto_width(root, box_model, containing_width);
    } else {
        box_model.content_width = root->style.width.to_pixels(containing_width);
    }
    apply_min_max_constraints(root, containing_width, containing_height);
    box_model.content_width = root->content_box.width;
    result.content_width = box_model.content_width;

    BlockContext context;
    context.containing_block_width = box_model.content_width;
    context.containing_block_height = containing_height;
    context.current_y = 0;

    layout_block_flow(root, context);

    result.content_height = context.current_y;
    result.total_width = box_model.get_total_width();
    result.total_height = box_model.margin_top + box_model.margin_bottom +
                          box_model.border_top + box_model.border_bottom +
                          box_model.padding_top + box_model.padding_bottom + context.current_y;

    root->content_box.width = box_model.content_width;
    root->content_box.height = result.content_height;
    return result;
}

void BlockLayout::layout_block_flow(const std::shared_ptr<LayoutNode>& parent, BlockContext& context) {
    if (!parent) return;
    BoxModel box_model = calculate_box_model(parent->style, context.containing_block_width, context.containing_block_height);
    position_children(parent, box_model, context);
}

} // namespace hre::layout
