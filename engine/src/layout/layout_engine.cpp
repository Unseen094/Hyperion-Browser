#include <hre/layout/layout_engine.hpp>
#include <hre/style/style_engine.hpp>
#include <hyperion/ui/renderer.hpp>
#include <algorithm>
#include <dwrite_3.h>
#include <wrl/client.h>

namespace hre::layout {

layout_node layout_engine::compute_layout(dom::node* root, float viewport_width, hyperion::ui::renderer& r) {
    layout_node root_layout;
    root_layout.dom_node = root;
    float current_y = 0.0f;
    
    // Initial pass: compute block-level positions and dimensions
    layout_recursive(root, current_y, viewport_width, r, root_layout);
    
    return root_layout;
}

void layout_engine::layout_recursive(dom::node* node, float& current_y, float viewport_width, hyperion::ui::renderer& r, layout_node& out_layout) {
    if (!node) return;

    auto it = style::g_computed_styles.find(node);
    if (it == style::g_computed_styles.end()) {
        style::computed_style default_style;
        style::g_computed_styles[node] = default_style;
        it = style::g_computed_styles.find(node);
    }
    auto& style = it->second;

    // 1. Skip non-rendered nodes
    if (style.display == L"none") {
        return;
    }

    out_layout.dom_node = node;

    if (node->type() == dom::node_type::element) {
        auto* el = static_cast<dom::element*>(node);
        
        // 2. Fetch Detailed Box Model Properties
        float mt = style.margin_top, mr = style.margin_right, mb = style.margin_bottom, ml = style.margin_left;
        float pt = style.padding_top, pr = style.padding_right, pb = style.padding_bottom, pl = style.padding_left;
        float bt = style.border_top, br = style.border_right, bb = style.border_bottom, bl = style.border_left;
        
        float styled_width = style.width;
        float styled_height = style.height;

        // 3. Handle Flexbox specifically
        if (style.display == L"flex") {
            layout_flex(node, current_y, viewport_width, r, out_layout);
            return;
        }

        // 4. Block Layout Foundation
        if (style.display == L"block") {
            current_y += mt;
        }

        out_layout.dimensions.x = ml;
        out_layout.dimensions.y = current_y;
        
        // Auto-width for block elements if not specified
        float available_width = viewport_width - ml - mr;
        out_layout.dimensions.width = (styled_width > 0) ? styled_width : available_width;
        
        out_layout.dimensions.margin = { ml, mr, mt, mb };
        out_layout.dimensions.padding = { pl, pr, pt, pb };
        out_layout.dimensions.border = { bl, br, bt, bb };

        float start_y = current_y;
        float children_y = start_y + bt + pt;
        float children_start_y = children_y;
        
        // Tracking for inline-flow (simple line breaking)
        float current_line_x = 0.0f;
        float current_line_height = 0.0f;

        for (const auto& child : node->children()) {
            // Ensure child has a style entry
            auto child_it = style::g_computed_styles.find(child.get());
            if (child_it == style::g_computed_styles.end()) {
                style::computed_style default_style;
                style::g_computed_styles[child.get()] = default_style;
                child_it = style::g_computed_styles.find(child.get());
            }
            const auto& child_style = child_it->second;
            if (child_style.display == L"none") continue;

            std::wstring child_display = child_style.display;
            if (child->type() == dom::node_type::text) child_display = L"inline";

            layout_node child_layout;
            
            if (child_display == L"inline") {
                // Inline layout logic: wrap text/inline boxes
                float inline_avail = out_layout.dimensions.content_width() - current_line_x;
                layout_recursive(child.get(), children_y, inline_avail, r, child_layout);
                
                child_layout.dimensions.x = current_line_x + (bl + pl);
                child_layout.dimensions.y = children_y;
                
                current_line_x += child_layout.dimensions.width;
                current_line_height = (std::max)(current_line_height, child_layout.dimensions.height);
                
                // Simple wrapping
                if (current_line_x > out_layout.dimensions.content_width()) {
                    current_line_x = 0;
                    children_y += current_line_height;
                    current_line_height = 0;
                    // Reposition child on new line
                    child_layout.dimensions.x = bl + pl;
                    child_layout.dimensions.y = children_y;
                    current_line_x = child_layout.dimensions.width;
                }
            } else {
                // Block layout logic: push to next line
                if (current_line_x > 0) {
                    children_y += current_line_height;
                    current_line_x = 0;
                    current_line_height = 0;
                }
                
                layout_recursive(child.get(), children_y, out_layout.dimensions.content_width(), r, child_layout);
                child_layout.dimensions.x += (bl + pl);
            }
            
            out_layout.children.push_back(std::move(child_layout));
        }
        
        children_y += current_line_height;

        // 5. Calculate Final Height
        if (styled_height > 0) {
            out_layout.dimensions.height = styled_height;
        } else {
            out_layout.dimensions.height = (children_y - children_start_y) + (bt + pt + bb + pb);
        }

        // Special handling for replaced elements (img, etc.)
        if (el->tag_name() == L"img") {
            if (styled_width <= 0) out_layout.dimensions.width = 100.0f;
            if (styled_height <= 0) out_layout.dimensions.height = 100.0f;
        }

        current_y = start_y + out_layout.dimensions.height;
        if (style.display == L"block") current_y += mb;

    } else if (node->type() == dom::node_type::text) {
        // 6. Text Layout (DirectWrite)
        auto* text_n = static_cast<dom::text_node*>(node);
        
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        r.write_factory()->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &format);
            
        Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
        r.write_factory()->CreateTextLayout(text_n->text().c_str(), (UINT32)text_n->text().length(),
            format.Get(), viewport_width, 1000.0f, &text_layout);
            
        DWRITE_TEXT_METRICS text_metrics;
        text_layout->GetMetrics(&text_metrics);

        out_layout.dimensions.x = 0;
        out_layout.dimensions.y = current_y;
        out_layout.dimensions.width = text_metrics.width;
        out_layout.dimensions.height = text_metrics.height;
    }
}

void layout_engine::layout_flex(dom::node* node, float& current_y, float viewport_width, hyperion::ui::renderer& r, layout_node& out_layout) {
    auto& style = style::g_computed_styles[node];
    out_layout.dom_node = node;
    
    float mt = style.margin_top, mr = style.margin_right, mb = style.margin_bottom, ml = style.margin_left;
    float pt = style.padding_top, pr = style.padding_right, pb = style.padding_bottom, pl = style.padding_left;
    float bt = style.border_top, br = style.border_right, bb = style.border_bottom, bl = style.border_left;

    float start_y = current_y;
    current_y += mt;

    float available_width = viewport_width - ml - mr;
    float content_width = (style.width > 0) ? style.width : available_width;
    
    out_layout.dimensions.x = ml;
    out_layout.dimensions.y = current_y;
    out_layout.dimensions.width = content_width;
    out_layout.dimensions.margin = { ml, mr, mt, mb };
    out_layout.dimensions.padding = { pl, pr, pt, pb };
    out_layout.dimensions.border = { bl, br, bt, bb };

    float flex_x = bl + pl;
    float flex_y = bt + pt;
    float max_line_height = 0;
    float current_line_width = 0;

    if (node->type() == dom::node_type::element) {
        auto* el = static_cast<dom::element*>(node);
        for (auto& child : el->children()) {
            if (style::g_computed_styles[child.get()].display == L"none") continue;

            layout_node child_layout;
            float dummy_y = 0; 
            layout_recursive(child.get(), dummy_y, out_layout.dimensions.content_width(), r, child_layout);
            
            if (style.flex_direction == L"row") {
                child_layout.dimensions.x = flex_x + current_line_width;
                child_layout.dimensions.y = flex_y;
                current_line_width += child_layout.dimensions.width;
                max_line_height = (std::max)(max_line_height, child_layout.dimensions.height);
            } else {
                child_layout.dimensions.x = flex_x;
                child_layout.dimensions.y = flex_y;
                flex_y += child_layout.dimensions.height;
                max_line_height += child_layout.dimensions.height;
            }
            out_layout.children.push_back(std::move(child_layout));
        }
    }

    if (style.height > 0) {
        out_layout.dimensions.height = style.height;
    } else {
        out_layout.dimensions.height = max_line_height + (bt + pt + bb + pb);
    }

    current_y = start_y + out_layout.dimensions.height + mb + mt;
}

layout_node* layout_engine::hit_test(layout_node& root, float x, float y) {
    bool inside = (x >= root.dimensions.x && x <= root.dimensions.x + root.dimensions.width &&
                  y >= root.dimensions.y && y <= root.dimensions.y + root.dimensions.height);
    
    if (!inside) return nullptr;

    for (auto it = root.children.rbegin(); it != root.children.rend(); ++it) {
        auto* found = hit_test(*it, x, y);
        if (found) return found;
    }

    return &root;
}

} // namespace hre::layout
