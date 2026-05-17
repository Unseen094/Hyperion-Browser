#include <hre/render/painter.hpp>
#include <hre/dom/node.hpp>
#include <hre/style/style_engine.hpp>
#include <hre/render/image_loader.hpp>
#include <cmath>
#include <dwrite.h>
#include <algorithm>

namespace hre::render {

// Helper to convert CSS colors to D2D
D2D1_COLOR_F css_to_d2d(const std::wstring& color) {
    if (color.empty() || color == L"transparent") return D2D1::ColorF(0, 0, 0, 0);
    if (color == L"red") return D2D1::ColorF(D2D1::ColorF::Red);
    if (color == L"blue") return D2D1::ColorF(D2D1::ColorF::Blue);
    if (color == L"green") return D2D1::ColorF(D2D1::ColorF::Green);
    if (color == L"black") return D2D1::ColorF(D2D1::ColorF::Black);
    if (color == L"white") return D2D1::ColorF(D2D1::ColorF::White);
    if (color == L"gray") return D2D1::ColorF(D2D1::ColorF::Gray);
    if (color == L"dark-gray") return D2D1::ColorF(0.2f, 0.2f, 0.25f, 1.0f);
    if (color == L"silver") return D2D1::ColorF(0.75f, 0.75f, 0.75f, 1.0f);
    if (color == L"yellow") return D2D1::ColorF(D2D1::ColorF::Yellow);
    
    // Handle hex colors #RGB or #RRGGBB
    if (color.length() == 4 && color[0] == L'#') {
        auto hex_digit = [](wchar_t c) { return (c >= L'0' && c <= L'9') ? (c - L'0') : (c >= L'a' && c <= L'f') ? (c - L'a' + 10) : (c >= L'A' && c <= L'F') ? (c - L'A' + 10) : 0; };
        float r = hex_digit(color[1]) / 15.0f;
        float g = hex_digit(color[2]) / 15.0f;
        float b = hex_digit(color[3]) / 15.0f;
        return D2D1::ColorF(r, g, b, 1.0f);
    }
    if (color.length() == 7 && color[0] == L'#') {
        auto hex_pair = [](wchar_t hi, wchar_t lo) {
            auto hex_digit = [](wchar_t c) { return (c >= L'0' && c <= L'9') ? (c - L'0') : (c >= L'a' && c <= L'f') ? (c - L'a' + 10) : (c >= L'A' && c <= L'F') ? (c - L'A' + 10) : 0; };
            return (hex_digit(hi) * 16 + hex_digit(lo)) / 255.0f;
        };
        return D2D1::ColorF(hex_pair(color[1], color[2]), hex_pair(color[3], color[4]), hex_pair(color[5], color[6]), 1.0f);
    }

    return D2D1::ColorF(0.2f, 0.2f, 0.2f, 1.0f); // Default dark gray
}

void painter::paint(const layout::layout_node& root, hyperion::ui::renderer& r) {
    paint_recursive(root, r);
}

#ifdef DrawText
#undef DrawText
#endif

void painter::paint_recursive(const layout::layout_node& node, hyperion::ui::renderer& r) {
    auto* context = r.context();
    if (!context) return;

    if (node.dom_node->type() == dom::node_type::element) {
        auto* el = static_cast<dom::element*>(node.dom_node);
        const auto& s = style::g_computed_styles[node.dom_node];

        if (s.display == L"none" || s.visibility == L"hidden") return;

        D2D1_RECT_F rect = D2D1::RectF(
            node.dimensions.x,
            node.dimensions.y,
            node.dimensions.x + node.dimensions.width,
            node.dimensions.y + node.dimensions.height
        );

        // 1. Box Shadow (Foundation)
        if (s.box_shadow_blur > 0) {
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> shadow_brush;
            D2D1_COLOR_F shadow_color = css_to_d2d(s.box_shadow_color);
            context->CreateSolidColorBrush(shadow_color, &shadow_brush);
            
            D2D1_RECT_F shadow_rect = rect;
            shadow_rect.left += s.box_shadow_offset_x;
            shadow_rect.top += s.box_shadow_offset_y;
            shadow_rect.right += s.box_shadow_offset_x;
            shadow_rect.bottom += s.box_shadow_offset_y;
            
            context->FillRectangle(shadow_rect, shadow_brush.Get());
        }

        // 2. Background
        if (s.background_color != L"transparent") {
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bg_brush;
            context->CreateSolidColorBrush(css_to_d2d(s.background_color), &bg_brush);
            
            if (s.border_radius > 0) {
                context->FillRoundedRectangle(D2D1::RoundedRect(rect, s.border_radius, s.border_radius), bg_brush.Get());
            } else {
                context->FillRectangle(rect, bg_brush.Get());
            }
        }

        // 3. Image Rendering
        if (el->tag_name() == L"img") {
            std::wstring src = el->get_attribute(L"src");
            if (!src.empty()) {
                auto bitmap = image_loader::load_from_url(src, context);
                if (bitmap) context->DrawBitmap(bitmap.Get(), rect);
            }
        }

        // 4. Border
        float bw = s.border_top; // Simplified for now
        if (bw > 0 && s.border_color != L"transparent") {
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border_brush;
            context->CreateSolidColorBrush(css_to_d2d(s.border_color), &border_brush);
            
            if (s.border_radius > 0) {
                context->DrawRoundedRectangle(D2D1::RoundedRect(rect, s.border_radius, s.border_radius), border_brush.Get(), bw);
            } else {
                context->DrawRectangle(rect, border_brush.Get(), bw);
            }
        }

    } else if (node.dom_node->type() == dom::node_type::text) {
        auto* text_node = static_cast<dom::text_node*>(node.dom_node);
        auto& parent_style = style::g_computed_styles[node.dom_node->parent()];

        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        r.write_factory()->CreateTextFormat(
            parent_style.font_family.empty() ? L"Segoe UI" : parent_style.font_family.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            parent_style.font_size > 0 ? parent_style.font_size : 14.0f,
            L"en-us",
            &format
        );

        // Text rendering - using GDI fallback for compatibility
        // Create GDI-compatible DC and draw
        HDC hdc = CreateCompatibleDC(nullptr);
        if (hdc) {
            std::wstring font_name = parent_style.font_family.empty() ? L"Segoe UI" : parent_style.font_family;
            float font_size = parent_style.font_size > 0 ? parent_style.font_size : 14.0f;
            int log_pixels_y = 96;
            HFONT hFont = CreateFontW((int)(font_size * log_pixels_y / 72), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, font_name.c_str());
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            RECT text_rect = {(LONG)node.dimensions.x, (LONG)node.dimensions.y,
                (LONG)(node.dimensions.x + node.dimensions.width),
                (LONG)(node.dimensions.y + node.dimensions.height)};
            DrawTextW(hdc, text_node->text().c_str(), -1, &text_rect, DT_LEFT | DT_TOP);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
            DeleteDC(hdc);
        }
    }

    // Paint children
    for (const auto& child : node.children) {
        paint_recursive(child, r);
    }
}

} // namespace hre::render
