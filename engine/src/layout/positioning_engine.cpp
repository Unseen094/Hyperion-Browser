#define NOMINMAX
#include "hre/layout/positioning_engine.hpp"
#include "hre/layout/layout_engine.hpp"
#include <algorithm>
#include <cmath>

namespace hre::layout {

bool PositioningEngine::is_positioned(const std::shared_ptr<LayoutNode>& node) {
    return node->style.position == L"relative" ||
           node->style.position == L"absolute" ||
           node->style.position == L"fixed" ||
           node->style.position == L"sticky";
}

double PositioningEngine::get_position_offset(const std::wstring& side, const css::ComputedStyle& style, double containing_size) {
    if (side == L"top") return style.top.to_pixels(containing_size);
    if (side == L"right") return style.right.to_pixels(containing_size);
    if (side == L"bottom") return style.bottom.to_pixels(containing_size);
    if (side == L"left") return style.left.to_pixels(containing_size);
    return 0;
}

std::shared_ptr<LayoutNode> PositioningEngine::find_containing_block(std::shared_ptr<LayoutNode> node) {
    auto current = node->parent;
    while (current) {
        auto pos = current->style.position;
        if (pos == L"relative" || pos == L"absolute" || pos == L"fixed" || pos == L"sticky" ||
            pos == L"static") {
            if (pos != L"static") return current;
        }
        // Find nearest positioned ancestor for absolute
        if (node->style.position == L"absolute") {
            if (is_positioned(current)) return current;
        }
        current = current->parent;
    }
    return nullptr;
}

void PositioningEngine::resolve_auto_offsets(std::shared_ptr<LayoutNode>& node, double containing_width, double containing_height) {
    auto& s = node->style;
    bool top_auto = s.top.unit == css::CssUnit::Auto;
    bool bottom_auto = s.bottom.unit == css::CssUnit::Auto;
    bool left_auto = s.left.unit == css::CssUnit::Auto;
    bool right_auto = s.right.unit == css::CssUnit::Auto;

    if (left_auto && right_auto) {
        // Center horizontally
        double auto_left = (containing_width - node->content_box.width) / 2.0;
        if (auto_left < 0) auto_left = 0;
        node->content_box.x = auto_left;
    } else if (left_auto) {
        double right = get_position_offset(L"right", s, containing_width);
        node->content_box.x = containing_width - node->content_box.width - right;
    } else {
        node->content_box.x = get_position_offset(L"left", s, containing_width);
    }

    if (top_auto && bottom_auto) {
        double auto_top = (containing_height - node->content_box.height) / 2.0;
        if (auto_top < 0) auto_top = 0;
        node->content_box.y = auto_top;
    } else if (top_auto) {
        double bottom = get_position_offset(L"bottom", s, containing_height);
        node->content_box.y = containing_height - node->content_box.height - bottom;
    } else {
        node->content_box.y = get_position_offset(L"top", s, containing_height);
    }
}

void PositioningEngine::layout_absolute(std::shared_ptr<LayoutNode> node, double containing_width, double containing_height) {
    if (!node) return;

    BoxModel box = calculate_box_model(node->style, containing_width, containing_height);

    if (node->style.width.unit == css::CssUnit::Auto) {
        node->content_box.width = box.content_width;
        if (node->content_box.width <= 0) {
            node->content_box.width = containing_width * 0.8;
        }
    } else {
        node->content_box.width = node->style.width.to_pixels(containing_width);
    }

    if (node->style.height.unit == css::CssUnit::Auto) {
        node->content_box.height = box.content_height;
        if (node->content_box.height <= 0) {
            // Auto-height from content
            auto display = node->style.display;
            if (display == L"flex" || display == L"inline-flex") {
                FlexboxEngine flex;
                flex.layout_flex_container(node, node->content_box.width, containing_height);
            } else if (display.find(L"table") == 0) {
                TableEngine tbl;
                tbl.layout_table(node, node->content_box.width, containing_height);
            } else {
                BlockLayout blk;
                auto r = blk.layout(node, node->content_box.width, containing_height);
                node->content_box.height = r.content_height;
            }
        }
    } else {
        node->content_box.height = node->style.height.to_pixels(containing_height);
    }

    resolve_auto_offsets(node, containing_width, containing_height);
}

void PositioningEngine::layout_fixed(std::shared_ptr<LayoutNode> node, double viewport_width, double viewport_height) {
    layout_absolute(node, viewport_width, viewport_height);
}

void PositioningEngine::layout_sticky(std::shared_ptr<LayoutNode> node, double scroll_offset_x, double scroll_offset_y) {
    // Sticky positioning: element is in-flow until scroll threshold reached
    if (!node) return;

    const auto& s = node->style;

    double original_x = node->content_box.x;
    double original_y = node->content_box.y;

    if (s.top.unit != css::CssUnit::Auto) {
        double top_offset = s.top.to_pixels(0);
        double sticky_y = std::max(scroll_offset_y + top_offset, original_y);
        // Clamp so bottom edge doesn't go past containing block bottom
        auto cb = find_containing_block(node);
        if (cb) {
            double cb_bottom = cb->content_box.y + cb->content_box.height;
            sticky_y = std::min(sticky_y, cb_bottom - node->content_box.height - s.bottom.to_pixels(0));
        }
        node->content_box.y = sticky_y;
    } else if (s.bottom.unit != css::CssUnit::Auto) {
        double bottom_offset = s.bottom.to_pixels(0);
        double sticky_y = std::min(original_y, scroll_offset_y - bottom_offset - node->content_box.height);
        auto cb = find_containing_block(node);
        if (cb) {
            double cb_top = cb->content_box.y;
            sticky_y = std::max(sticky_y, cb_top + s.top.to_pixels(0));
        }
        node->content_box.y = sticky_y;
    }

    if (s.left.unit != css::CssUnit::Auto) {
        double left_offset = s.left.to_pixels(0);
        double sticky_x = std::max(scroll_offset_x + left_offset, original_x);
        auto cb = find_containing_block(node);
        if (cb) {
            double cb_right = cb->content_box.x + cb->content_box.width;
            sticky_x = std::min(sticky_x, cb_right - node->content_box.width - s.right.to_pixels(0));
        }
        node->content_box.x = sticky_x;
    } else if (s.right.unit != css::CssUnit::Auto) {
        double right_offset = s.right.to_pixels(0);
        double sticky_x = std::min(original_x, scroll_offset_x - right_offset - node->content_box.width);
        auto cb = find_containing_block(node);
        if (cb) {
            double cb_left = cb->content_box.x;
            sticky_x = std::max(sticky_x, cb_left + s.left.to_pixels(0));
        }
        node->content_box.x = sticky_x;
    }
}

void PositioningEngine::compute_stacking_contexts(std::shared_ptr<LayoutNode> root, std::vector<StackingContext>& contexts, bool is_root) {
    if (!root) return;

    int z = root->style.z_index;
    bool creates_context = is_root ||
                           (z != 0 && is_positioned(root)) ||
                           root->style.opacity.number < 1.0 ||
                           root->style.transform != L"none";

    if (creates_context) {
        StackingContext ctx;
        ctx.node = root;
        ctx.z_index = z;
        ctx.is_root = is_root;

        // Collect child contexts
        for (auto& child : root->children) {
            compute_stacking_contexts(child, ctx.children, false);
        }

        contexts.push_back(ctx);
    } else {
        for (auto& child : root->children) {
            compute_stacking_contexts(child, contexts, false);
        }
    }
}

void PositioningEngine::sort_stacking_contexts(std::vector<StackingContext>& contexts) {
    std::sort(contexts.begin(), contexts.end(), [](const StackingContext& a, const StackingContext& b) {
        return a.z_index < b.z_index;
    });
}

void PositioningEngine::paint_stacking_order(const std::vector<StackingContext>& contexts) {
    // CSS 2.2 paint order:
    // 1. Background/border of root
    // 2. Negative z-index
    // 3. In-flow, non-inline, non-positioned descendants
    // 4. Non-positioned floats
    // 5. In-flow, inline, non-positioned descendants
    // 6. z-index: auto / 0
    // 7. Positive z-index

    // Sort contexts by z-index for correct paint order
    for (const auto& ctx : contexts) {
        // Paint order is determined by z-index sort
        if (!ctx.children.empty()) {
            std::vector<StackingContext> sorted = ctx.children;
            sort_stacking_contexts(sorted);
            paint_stacking_order(sorted);
        }
    }
}

void PositioningEngine::layout_positioned(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    if (!node) return;

    auto pos = node->style.position;

    if (pos == L"absolute") {
        auto cb = find_containing_block(node);
        double cb_w = cb ? cb->content_box.width : available_width;
        double cb_h = cb ? cb->content_box.height : available_height;
        layout_absolute(node, cb_w, cb_h);
    } else if (pos == L"fixed") {
        layout_fixed(node, available_width, available_height);
    } else if (pos == L"sticky") {
        layout_sticky(node, 0, 0); // Called with current scroll offset
    }

    // Handle relative positioning offsets
    if (pos == L"relative") {
        double left = get_position_offset(L"left", node->style, available_width);
        double top = get_position_offset(L"top", node->style, available_height);
        double right = get_position_offset(L"right", node->style, available_width);
        double bottom = get_position_offset(L"bottom", node->style, available_height);

        if (left != 0) node->content_box.x += left;
        else if (right != 0) node->content_box.x -= right;

        if (top != 0) node->content_box.y += top;
        else if (bottom != 0) node->content_box.y -= bottom;
    }
}

} // namespace hre::layout
