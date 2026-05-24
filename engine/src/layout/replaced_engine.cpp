#define NOMINMAX
#include "hre/layout/replaced_engine.hpp"
#include "hre/layout/layout_engine.hpp"
#include "hre/css/style_engine.hpp"

namespace hre::layout {

void ReplacedEngine::layout_replaced(std::shared_ptr<LayoutNode> node, double available_width, double available_height) {
    if (!node) return;
    
    // Check if explicit CSS width/height are provided
    bool has_explicit_width = node->style.width.unit != css::CssUnit::Auto;
    bool has_explicit_height = node->style.height.unit != css::CssUnit::Auto;
    
    // Default intrinsic dimensions for replaced elements
    double intrinsic_width = 300.0;  // Default image size
    double intrinsic_height = 150.0;
    double intrinsic_ratio = intrinsic_width / intrinsic_height;
    
    if (has_explicit_width && has_explicit_height) {
        // Both dimensions specified - use them directly
        node->content_box.width = node->style.width.to_pixels(available_width);
        node->content_box.height = node->style.height.to_pixels(available_height);
    } else if (has_explicit_width) {
        // Only width specified - calculate height from intrinsic ratio
        node->content_box.width = node->style.width.to_pixels(available_width);
        node->content_box.height = node->content_box.width / intrinsic_ratio;
    } else if (has_explicit_height) {
        // Only height specified - calculate width from intrinsic ratio
        node->content_box.height = node->style.height.to_pixels(available_height);
        node->content_box.width = node->content_box.height * intrinsic_ratio;
    } else {
        // Neither specified - use intrinsic dimensions
        node->content_box.width = intrinsic_width;
        node->content_box.height = intrinsic_height;
    }
    
    // Clamp to available space
    node->content_box.width = std::min(node->content_box.width, available_width);
    node->content_box.height = std::min(node->content_box.height, available_height);
}

} // namespace hre::layout