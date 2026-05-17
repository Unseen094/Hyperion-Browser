#pragma once

#include <hre/dom/node.hpp>
#include <vector>

namespace hyperion::ui { class renderer; }

namespace hre::layout {

struct edges {
    float left = 0, right = 0, top = 0, bottom = 0;
};

struct box_rect {
    float x = 0, y = 0, width = 0, height = 0;
    edges margin;
    edges border;
    edges padding;

    // Returns the area inside the padding
    float content_width() const { return width - padding.left - padding.right - border.left - border.right; }
    float content_height() const { return height - padding.top - padding.bottom - border.top - border.bottom; }
};

struct layout_node {
    dom::node* dom_node;
    box_rect dimensions;
    std::vector<layout_node> children;
};

class layout_engine {
public:
    layout_node compute_layout(dom::node* root, float viewport_width, hyperion::ui::renderer& r);
    layout_node* hit_test(layout_node& root, float x, float y);

private:
    void layout_recursive(dom::node* node, float& current_y, float viewport_width, hyperion::ui::renderer& r, layout_node& out_layout);
    void layout_flex(dom::node* node, float& current_y, float viewport_width, hyperion::ui::renderer& r, layout_node& out_layout);
};

} // namespace hre::layout
