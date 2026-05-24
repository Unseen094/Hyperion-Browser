#pragma once

#include "hre/css/style_engine.hpp"
#include <cmath>

namespace hre::layout {

// Layout Box (Rect)
struct LayoutRect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;

    LayoutRect() = default;
    LayoutRect(double x, double y, double w, double h) : x(x), y(y), width(w), height(h) {}
};

// Box Model Components
struct BoxModel {
    double margin_top = 0;
    double margin_right = 0;
    double margin_bottom = 0;
    double margin_left = 0;
    
    double border_top = 0;
    double border_right = 0;
    double border_bottom = 0;
    double border_left = 0;
    
    double padding_top = 0;
    double padding_right = 0;
    double padding_bottom = 0;
    double padding_left = 0;
    
    double content_width = 0;
    double content_height = 0;
    
    // Calculated boxes
    double get_padding_width() const { return padding_left + padding_right; }
    double get_padding_height() const { return padding_top + padding_bottom; }
    double get_border_width() const { return border_left + border_right; }
    double get_border_height() const { return border_top + border_bottom; }
    double get_margin_width() const { return margin_left + margin_right; }
    double get_margin_height() const { return margin_top + margin_bottom; }
    
    double get_total_width() const {
        return margin_left + border_left + padding_left + content_width + padding_right + border_right + margin_right;
    }
    
    double get_total_height() const {
        return margin_top + border_top + padding_top + content_height + padding_bottom + border_bottom + margin_bottom;
    }
    
    double get_inner_width() const {
        return content_width + padding_left + padding_right;
    }
    
    double get_inner_height() const {
        return content_height + padding_top + padding_bottom;
    }
};

// Helper to calculate box model from computed style
inline BoxModel calculate_box_model(const css::ComputedStyle& style, double containing_block_width, double containing_block_height) {
    BoxModel box;
    
    // Convert style values to pixels
    box.margin_top = style.margin_top.to_pixels(containing_block_width);
    box.margin_right = style.margin_right.to_pixels(containing_block_width);
    box.margin_bottom = style.margin_bottom.to_pixels(containing_block_width);
    box.margin_left = style.margin_left.to_pixels(containing_block_width);
    
    box.border_top = style.border_top_width.to_pixels(containing_block_width);
    box.border_right = style.border_right_width.to_pixels(containing_block_width);
    box.border_bottom = style.border_bottom_width.to_pixels(containing_block_width);
    box.border_left = style.border_left_width.to_pixels(containing_block_width);
    
    box.padding_top = style.padding_top.to_pixels(containing_block_width);
    box.padding_right = style.padding_right.to_pixels(containing_block_width);
    box.padding_bottom = style.padding_bottom.to_pixels(containing_block_width);
    box.padding_left = style.padding_left.to_pixels(containing_block_width);
    
    // Width calculation
    if (style.width.unit == css::CssUnit::Auto) {
        box.content_width = containing_block_width - box.padding_left - box.padding_right - 
                           box.border_left - box.border_right;
        if (box.content_width < 0) box.content_width = 0;
    } else {
        box.content_width = style.width.to_pixels(containing_block_width);
    }
    
    // Height calculation (often auto)
    if (style.height.unit == css::CssUnit::Auto) {
        box.content_height = 0; // Will be calculated based on content
    } else {
        box.content_height = style.height.to_pixels(containing_block_height);
    }
    
    return box;
}

} // namespace hre::layout
