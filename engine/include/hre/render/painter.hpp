#pragma once

#include <hre/layout/layout_engine.hpp>
#include <hyperion/ui/renderer.hpp>
#include <hre/text/text_shaping.hpp>
#include <memory>
#include <vector>
#include <string>
#include <dwrite.h>
#include <d2d1.h>

namespace hre::render {

class painter {
public:
    void paint(const layout::LayoutNode& root, hyperion::ui::renderer& r);

private:
    void paint_recursive(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    
    // Text rendering methods
    void paint_text(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_text_with_shaping(const std::wstring& text, 
                                const layout::LayoutNode& node, 
                                hyperion::ui::renderer& r);
    
    // Gradient rendering methods
    void paint_gradients(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_linear_gradient(const layout::LayoutNode& node, 
                              const std::wstring& gradient_spec, 
                              hyperion::ui::renderer& r);
    void paint_radial_gradient(const layout::LayoutNode& node, 
                              const std::wstring& gradient_spec, 
                              hyperion::ui::renderer& r);
    
    // Border rendering methods
    void paint_borders(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_solid_border(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_dashed_border(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_dotted_border(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_double_border(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    
    // Background rendering methods
    void paint_background(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_solid_background(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    void paint_gradient_background(const layout::LayoutNode& node, hyperion::ui::renderer& r);
    
    // Helper methods for creating brushes
    Microsoft::WRL::ComPtr<ID2D1Brush> create_linear_gradient_brush(
        const D2D1_RECT_F& rect,
        const std::wstring& gradient_spec,
        ID2D1RenderTarget* renderTarget);
    Microsoft::WRL::ComPtr<ID2D1Brush> create_radial_gradient_brush(
        const D2D1_RECT_F& rect,
        const std::wstring& gradient_spec,
        ID2D1RenderTarget* renderTarget);
    
    // Helper method for color conversion
    D2D1_COLOR_F css_color_to_d2d(const std::wstring& color);
    
    // Helper method for gradient parsing
    std::vector<std::pair<D2D1_COLOR_F, float>> parse_gradient_stops(const std::wstring& gradient);
};

} // namespace hre::render
