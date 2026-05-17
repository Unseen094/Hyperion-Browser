#pragma once

#include <hre/layout/layout_engine.hpp>
#include <hyperion/ui/renderer.hpp>

namespace hre::render {

class painter {
public:
    void paint(const layout::layout_node& root, hyperion::ui::renderer& r);

private:
    void paint_recursive(const layout::layout_node& node, hyperion::ui::renderer& r);
};

} // namespace hre::render
