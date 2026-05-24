#pragma once

#include "hre/layout/box_model.hpp"
#include "hre/dom/node.hpp"
#include <memory>

namespace hre::layout {
struct LayoutNode;

struct InlineBox {
    std::shared_ptr<LayoutNode> node;
    double x = 0, y = 0, width = 0, height = 0;
    double baseline_offset = 0;
    int z_index = 0;
};

class InlineLayout {
public:
    InlineLayout() = default;

    void layout_inline_content(
        std::shared_ptr<LayoutNode> container,
        const BoxModel& box_model,
        double available_width,
        double& out_height
    );

    void layout_inline_block(
        std::shared_ptr<LayoutNode> node,
        double available_width,
        double& out_width,
        double& out_height,
        double& out_baseline
    );

private:
    double resolve_vertical_align(const std::wstring& val, double parent_line_height, double own_height);
    double determine_line_height(const std::shared_ptr<LayoutNode>& node);
};

} // namespace hre::layout
