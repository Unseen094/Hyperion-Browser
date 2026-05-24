#pragma once

#include <memory>

namespace hre::layout {
struct LayoutNode;

class OverflowEngine {
public:
    OverflowEngine() = default;

    void layout_overflow(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
};

} // namespace hre::layout
