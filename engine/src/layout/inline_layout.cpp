#include "hre/layout/inline_layout.hpp"
#include "hre/layout/layout_engine.hpp"
#include <algorithm>
#include <cmath>

namespace hre::layout {

double InlineLayout::resolve_vertical_align(const std::wstring& val, double parent_line_height, double own_height) {
    if (val == L"top") return 0;
    if (val == L"bottom") return parent_line_height - own_height;
    if (val == L"middle") return (parent_line_height - own_height) / 2.0;
    if (val == L"text-top" || val == L"super") return 0;
    if (val == L"text-bottom" || val == L"sub") return parent_line_height - own_height;
    // baseline (default)
    return 0;
}

double InlineLayout::determine_line_height(const std::shared_ptr<LayoutNode>& node) {
    const auto& lh = node->style.line_height;
    if (lh.unit == css::CssUnit::Pixel) return lh.number;
    if (lh.unit == css::CssUnit::Em || lh.unit == css::CssUnit::Rem) return lh.to_pixels(16.0);
    if (lh.unit == css::CssUnit::Percent) return lh.to_pixels(node->style.font_size.to_pixels(16.0));
    // normal
    return node->style.font_size.to_pixels(16.0) * 1.2;
}

void InlineLayout::layout_inline_block(
    std::shared_ptr<LayoutNode> node,
    double available_width,
    double& out_width,
    double& out_height,
    double& out_baseline
) {
    BoxModel box = calculate_box_model(node->style, available_width, 0);
    auto display = node->style.display;

    if (display == L"inline-block" || display == L"inline-flex" || display == L"inline-grid") {
        out_width = box.content_width;
        out_height = box.margin_top + box.border_top + box.padding_top + box.content_height + box.padding_bottom + box.border_bottom + box.margin_bottom;
        out_baseline = (out_height - box.border_bottom - box.padding_bottom) * 0.8;
    } else {
        out_width = box.content_width > 0 ? box.content_width : available_width;
        out_height = determine_line_height(node);
        out_baseline = out_height * 0.8;
    }
}

void InlineLayout::layout_inline_content(
    std::shared_ptr<LayoutNode> container,
    const BoxModel& box_model,
    double available_width,
    double& out_height
) {
    double line_height = determine_line_height(container);
    double current_x = box_model.padding_left;
    double current_y = box_model.padding_top;
    double max_line_height = line_height;
    std::vector<InlineBox> line_boxes;
    double text_width = 0;

    // Estimate text width (simplified)
    if (container->dom_node && !container->dom_node->text_content().empty()) {
        // Simple char-width estimation
        auto text = container->dom_node->text_content();
        text_width = text.length() * 7.0;
    }

    double remaining = available_width - current_x;
    if (text_width > remaining && remaining > 0) {
        text_width = remaining;
    }

    InlineBox text_box;
    text_box.x = current_x;
    text_box.y = current_y;
    text_box.width = text_width;
    text_box.height = line_height;
    text_box.baseline_offset = line_height * 0.8;
    line_boxes.push_back(text_box);
    current_x += text_width;

    // Layout child inline-level elements
    for (auto& child : container->children) {
        if (!child) continue;
        auto display = child->style.display;

        if (display == L"inline") {
            InlineLayout inline_child;
            double child_h = 0;
            inline_child.layout_inline_content(child, calculate_box_model(child->style, available_width, 0), available_width - current_x, child_h);
            InlineBox ib;
            ib.node = child;
            ib.x = current_x;
            ib.y = current_y;
            ib.width = child->content_box.width > 0 ? child->content_box.width : text_width;
            ib.height = child_h > 0 ? child_h : line_height;
            ib.baseline_offset = ib.height * 0.8;
            line_boxes.push_back(ib);
            current_x += ib.width;
            max_line_height = std::max(max_line_height, ib.height);
        } else if (display == L"inline-block" || display == L"inline-flex") {
            double w = 0, h = 0, bl = 0;
            layout_inline_block(child, available_width - current_x, w, h, bl);

            BoxModel child_box = calculate_box_model(child->style, available_width - current_x, 0);
            child->content_box.x = current_x + child_box.margin_left + child_box.border_left + child_box.padding_left;
            child->content_box.y = current_y + child_box.margin_top + child_box.border_top + child_box.padding_top;

            // Vertical-align
            double va_offset = resolve_vertical_align(std::wstring{}, max_line_height, h);
            child->content_box.y += va_offset;

            if (child->style.width.unit == css::CssUnit::Auto) {
                child->content_box.width = calculate_box_model(child->style, available_width - current_x, 0).content_width;
            }

            InlineBox ib;
            ib.node = child;
            ib.x = child->content_box.x;
            ib.y = child->content_box.y;
            ib.width = w;
            ib.height = h;
            ib.baseline_offset = bl + va_offset;
            line_boxes.push_back(ib);
            current_x += w + child_box.margin_left + child_box.margin_right;
            max_line_height = std::max(max_line_height, h + va_offset);
        }
    }

    // Distribute vertical-alignment
    for (auto& ib : line_boxes) {
        double va_offset = 0;
        if (ib.node) {
            va_offset = resolve_vertical_align(std::wstring{}, max_line_height, ib.height);
        }
        ib.y = current_y + va_offset;
        if (ib.node) {
            ib.node->content_box.y = ib.y;
        }
    }

    double total_height = current_y + max_line_height + box_model.padding_bottom;
    container->content_box.height = total_height;
    out_height = total_height;
}

} // namespace hre::layout
