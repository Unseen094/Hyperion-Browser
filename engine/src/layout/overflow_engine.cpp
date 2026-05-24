#include "hre/layout/overflow_engine.hpp"
#include "hre/layout/layout_engine.hpp"

namespace hre::layout {

void OverflowEngine::layout_overflow(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    if (!node) return;
    
    // Calculate content bounds
    double content_right = node->content_box.x + node->content_box.width;
    double content_bottom = node->content_box.y + node->content_box.height;
    
    // Available space within container
    double container_right = node->content_box.x + available_width;
    double container_bottom = node->content_box.y + available_height;
    
    // Check if content overflows
    bool overflows_x = content_right > container_right;
    bool overflows_y = content_bottom > container_bottom;
    
    // For overflow:hidden, we would set up clipping regions
    // For overflow:scroll/auto, we would calculate scrollbar dimensions
    // This is handled in the rendering phase
    
    // Clamp content box to available space for layout purposes
    if (node->content_box.width > available_width) {
        node->content_box.width = available_width;
    }
    if (node->content_box.height > available_height) {
        node->content_box.height = available_height;
    }
}

} // namespace hre::layout