#pragma once

#include <memory>

namespace hre::layout {
struct LayoutNode;

class ReplacedEngine {
public:
    ReplacedEngine() = default;

    void layout_replaced(std::shared_ptr<LayoutNode> node, double available_width, double available_height);
};

} // namespace hre::layout
