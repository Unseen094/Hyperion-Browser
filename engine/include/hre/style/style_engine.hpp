#pragma once

#include <hre/dom/node.hpp>
#include <hre/css/css_parser.hpp>
#include <map>

namespace hre::style {

struct computed_style {
    std::wstring color = L"white";
    std::wstring background_color = L"transparent";
    std::wstring display = L"block"; // block, inline, flex, none

    float width = -1.0f;
    float height = -1.0f;
    float opacity = 1.0f;

    // Box Model
    float margin_top = 0.0f, margin_right = 0.0f, margin_bottom = 0.0f, margin_left = 0.0f;
    float padding_top = 0.0f, padding_right = 0.0f, padding_bottom = 0.0f, padding_left = 0.0f;
    float border_width = 0.0f;
    float border_top = 0.0f, border_right = 0.0f, border_bottom = 0.0f, border_left = 0.0f;

    std::wstring border_color = L"transparent";
    float border_radius = 0.0f;

    // Box Shadow
    std::wstring box_shadow_color = L"transparent";
    float box_shadow_offset_x = 0.0f;
    float box_shadow_offset_y = 0.0f;
    float box_shadow_blur = 0.0f;

    // Font Properties
    std::wstring font_family = L"Segoe UI";
    float font_size = 14.0f;
    std::wstring font_weight = L"normal";
    std::wstring font_style = L"normal";
    std::wstring text_align = L"left";
    float line_height = 1.2f;
    float letter_spacing = 0.0f;
    std::wstring text_overflow = L"clip"; // clip, ellipsis
    std::wstring text_decoration = L"none"; // none, underline, line-through
    std::wstring text_transform = L"none"; // none, uppercase, lowercase, capitalize

    // Flexbox
    std::wstring flex_direction = L"row";
    std::wstring justify_content = L"flex-start";
    std::wstring align_items = L"stretch";
    float flex_grow = 0.0f;
    float flex_shrink = 1.0f;

    // Positioning
    std::wstring position = L"static"; // static, relative, absolute, fixed
    float top = 0.0f, right = 0.0f, bottom = 0.0f, left = 0.0f;
    float z_index = 0.0f;

    // Visibility
    std::wstring visibility = L"visible"; // visible, hidden
    std::wstring overflow = L"visible"; // visible, hidden, scroll, auto

    // Transform
    std::wstring transform = L"";
    float transform_origin_x = 0.5f; // 50%
    float transform_origin_y = 0.5f;
};

class style_engine {
public:
    void apply_styles(dom::document* doc, const css::stylesheet& ss);

private:
    void style_recursive(dom::node* node, const css::stylesheet& ss);
    bool matches_selector(dom::element* el, const css::selector& sel);
};

extern std::map<dom::node*, computed_style> g_computed_styles;

} // namespace hre::style
