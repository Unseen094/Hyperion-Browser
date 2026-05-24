#include "hre/layout/layout_engine.hpp"
#include <algorithm>
#include <iostream>

namespace hre::layout {

LayoutEngine::LayoutEngine() {}

LayoutEngine::~LayoutEngine() {}

void LayoutEngine::load_css(const std::wstring& css) {
    style_engine.load_stylesheet(css);
}

void LayoutEngine::load_raw_rules(const std::vector<std::wstring>& rules) {
    style_engine.load_raw_rules(rules);
}

std::shared_ptr<LayoutNode> LayoutEngine::build_layout_tree(const dom::Node* root, const css::ComputedStyle& parent_style) {
    if (!root) return nullptr;

    auto layout_node = std::make_shared<LayoutNode>(root);
    layout_node->style = style_engine.compute_style(root, &parent_style);

    // Recursively build child_nodes
    for (const auto& child : root->child_nodes()) {
        if (child) {
            auto child_layout = build_layout_tree(child.get(), layout_node->style);
            if (child_layout) {
                child_layout->parent = layout_node;
                layout_node->children.push_back(child_layout);
            }
        }
    }

    return layout_node;
}

void LayoutEngine::layout(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    if (!node) return;

    // Calculate Box Model
    calculate_box_model(node, available_width);

    const std::wstring& display = node->style.display;
    const std::wstring& position = node->style.position;

    // Layout based on display type
    if (display == L"flex") {
        layout_flex(node, available_width, available_height);
    } else if (display == L"grid") {
        layout_grid(node, available_width, available_height);
    } else if (display == L"inline" || display == L"inline-block") {
        layout_inline(node, available_width, available_height);
    } else if (display == L"table" || display == L"inline-table") {
        layout_table(node, available_width, available_height);
    } else {
        // Default to block formatting context
        layout_block(node, available_width, available_height);
    }

    // Handle positioned elements (relative, absolute, fixed, sticky)
    if (position != L"static") {
        layout_positioned(node, available_width, available_height);
    }

    // Detect replaced elements (e.g., img, video, audio, canvas)
    bool is_replaced = false;
    if (auto* elem = dynamic_cast<const hre::dom::Element*>(node->dom_node)) {
        const std::wstring& tag = elem->tag_name();
        if (tag == L"img" || tag == L"video" || tag == L"audio" || tag == L"canvas") {
            is_replaced = true;
        }
    }
    if (is_replaced) {
        layout_replaced(node, available_width, available_height);
    }

    // Overflow & clipping (currently a stub)
    layout_overflow(node, available_width, available_height);
}

void LayoutEngine::layout_tree(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    if (!node) return;
    layout(node, available_width, available_height);

    for (auto& child : node->children) {
        double child_width = available_width - node->box_model.padding_left - node->box_model.padding_right;
        layout_tree(child, child_width, available_height);
    }
}

void LayoutEngine::calculate_box_model(std::shared_ptr<LayoutNode> node, double parent_width) {
    const auto& style = node->style;
    node->box_model = ::hre::layout::calculate_box_model(style, parent_width, 0);

    // Content Box
    node->content_box.width = node->box_model.content_width;
    node->content_box.height = node->box_model.content_height;

    // Padding Box
    node->padding_box.x = node->box_model.margin_left + node->box_model.border_left;
    node->padding_box.y = node->box_model.margin_top + node->box_model.border_top;
    node->padding_box.width = node->box_model.content_width + node->box_model.get_padding_width();
    node->padding_box.height = node->box_model.content_height + node->box_model.get_padding_height();

    // Border Box
    node->border_box.x = node->box_model.margin_left;
    node->border_box.y = node->box_model.margin_top;
    node->border_box.width = node->box_model.content_width + node->box_model.get_padding_width() + node->box_model.get_border_width();
    node->border_box.height = node->box_model.content_height + node->box_model.get_padding_height() + node->box_model.get_border_height();

    // Margin Box
    node->margin_box.x = 0;
    node->margin_box.y = 0;
    node->margin_box.width = node->border_box.width + node->box_model.get_margin_width();
    node->margin_box.height = node->border_box.height + node->box_model.get_margin_height();
}

void LayoutEngine::layout_block(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    // Use BlockLayout for proper block formatting context
    BlockLayoutResult result = block_layout.layout(node, available_width, available_height);
    node->content_box.width = result.content_width;
    node->content_box.height = result.content_height;
}

void LayoutEngine::layout_inline(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    double out_height = 0;
    inline_layout.layout_inline_content(node, node->box_model, available_width, out_height);
    node->content_box.height = out_height;
}

void LayoutEngine::layout_flex(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    flexbox_engine.layout_flex_container(node, available_width, available_height);
}

void LayoutEngine::layout_grid(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    grid_engine.layout_grid_container(node, available_width, available_height);
}

void LayoutEngine::layout_positioned(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    positioning_engine.layout_positioned(node, available_width, available_height);
}

void LayoutEngine::layout_table(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    table_engine.layout_table(node, available_width, available_height);
}

void LayoutEngine::layout_replaced(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    replaced_engine.layout_replaced(node, available_width, available_height);
}

void LayoutEngine::layout_overflow(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    overflow_engine.layout_overflow(node, available_width, available_height);
}

void LayoutEngine::layout_text_content(std::shared_ptr<LayoutNode> node, double available_width) {
    // Placeholder for future text layout within block/inline containers
    // Currently no special handling beyond box model
    (void)node;
    (void)available_width;
}

void LayoutEngine::update_style_for_node(std::shared_ptr<LayoutNode> node, const dom::Element* target,
                                          bool is_hovered, bool is_focused) {
    if (!node) return;

    if (node->dom_node == target) {
        const css::ComputedStyle* parent_style = nullptr;
        css::ComputedStyle parent_default;
        if (node->parent) {
            parent_style = &node->parent->style;
        } else {
            parent_default.display = L"block";
            parent_style = &parent_default;
        }
        node->style = style_engine.compute_style(node->dom_node, parent_style, is_hovered, is_focused);
        return;
    }

    for (auto& child : node->children) {
        update_style_for_node(child, target, is_hovered, is_focused);
    }
}

LayoutRect LayoutEngine::get_bounds(const std::shared_ptr<LayoutNode>& node) const {
    if (!node) return LayoutRect();
    return node->margin_box;
}

} // namespace hre::layout
