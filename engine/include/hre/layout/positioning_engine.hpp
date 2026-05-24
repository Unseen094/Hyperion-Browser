#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include "hre/css/style_engine.hpp"

namespace hre::layout {
struct LayoutNode;

struct StackingContext {
    std::shared_ptr<LayoutNode> node;
    int z_index = 0;
    bool is_root = false;
    std::vector<StackingContext> children;

    bool operator<(const StackingContext& other) const {
        return z_index < other.z_index;
    }
};

class PositioningEngine {
public:
    PositioningEngine() = default;

    void layout_positioned(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
    void layout_absolute(std::shared_ptr<LayoutNode> node, double containing_width, double containing_height);
    void layout_fixed(std::shared_ptr<LayoutNode> node, double viewport_width, double viewport_height);
    void layout_sticky(std::shared_ptr<LayoutNode> node, double scroll_offset_x, double scroll_offset_y);

    void compute_stacking_contexts(std::shared_ptr<LayoutNode> root, std::vector<StackingContext>& contexts, bool is_root = false);
    void sort_stacking_contexts(std::vector<StackingContext>& contexts);
    void paint_stacking_order(const std::vector<StackingContext>& contexts);

    std::shared_ptr<LayoutNode> find_containing_block(std::shared_ptr<LayoutNode> node);

private:
    void resolve_auto_offsets(std::shared_ptr<LayoutNode>& node, double containing_width, double containing_height);
    double get_position_offset(const std::wstring& side, const css::ComputedStyle& style, double containing_size);
    bool is_positioned(const std::shared_ptr<LayoutNode>& node);
};

} // namespace hre::layout
