#include <hre/render/display_list.hpp>
#include <hre/style/style_engine.hpp>

namespace hre::render {

void display_list::build_from_layout(const layout::LayoutNode& root) {
    clear();
    build_recursive(root);
}

void display_list::build_recursive(const layout::LayoutNode& node) {
    const auto& style = node.style;

    if (style.display == L"none" || style.visibility == L"hidden") {
        return;
    }

    float x = static_cast<float>(node.content_box.x);
    float y = static_cast<float>(node.content_box.y);
    float w = static_cast<float>(node.content_box.width);
    float h = static_cast<float>(node.content_box.height);

    if (node.dom_node->type() == dom::node_type::element) {
        const auto* el = static_cast<const dom::element*>(node.dom_node);

        if (style.box_shadow_blur > 0) {
            display_item shadow;
            shadow.kind = display_item::dl_item_type::SHADOW_ITEM;
            shadow.x = x + style.box_shadow_offset_x;
            shadow.y = y + style.box_shadow_offset_y;
            shadow.width = w;
            shadow.height = h;
            shadow.shadow_offset_x = style.box_shadow_offset_x;
            shadow.shadow_offset_y = style.box_shadow_offset_y;
            shadow.shadow_blur = style.box_shadow_blur;
            shadow.shadow_color = 0x80000000;
            m_items.push_back(shadow);
        }

        if (style.background_color != L"transparent" && style.background_color != L"") {
            if (style.border_radius > 0) {
                add_rounded_rect(x, y, w, h, style.border_radius, 0xFF000000);
            } else {
                add_solid_rect(x, y, w, h, 0xFF000000);
            }
        }

        if (el->tag_name() == L"img") {
            auto src = el->get_attribute(L"src");
            if (!src.empty()) {
                display_item img;
                img.kind = display_item::dl_item_type::DL_BITMAP;
                img.x = x;
                img.y = y;
                img.width = w;
                img.height = h;
                img.image_url = src;
                m_items.push_back(img);
            }
        }

        float bw = static_cast<float>(style.border_top);
        if (bw > 0 && style.border_color != L"transparent") {
            add_border(x, y, w, h, bw, 0xFF000000, style.border_radius);
        }
    }

    for (const auto& child : node.children) {
        build_recursive(*child);
    }
}

void display_list::add_solid_rect(float x, float y, float w, float h, uint32_t color) {
    display_item item;
    item.kind = display_item::dl_item_type::SOLID_RECT;
    item.x = x;
    item.y = y;
    item.width = w;
    item.height = h;
    item.bg_color = color;
    if (!m_clip_stack.empty()) {
        item.clip_id = m_clip_stack.back();
        item.is_clipped = true;
    }
    m_items.push_back(item);
}

void display_list::add_rounded_rect(float x, float y, float w, float h, float radius, uint32_t color) {
    display_item item;
    item.kind = display_item::dl_item_type::ROUNDED_RECT;
    item.x = x;
    item.y = y;
    item.width = w;
    item.height = h;
    item.border_radius = radius;
    item.bg_color = color;
    if (!m_clip_stack.empty()) {
        item.clip_id = m_clip_stack.back();
        item.is_clipped = true;
    }
    m_items.push_back(item);
}

void display_list::add_text_run(float x, float y, const std::wstring& text, const std::wstring& font, float size, uint32_t color) {
    display_item item;
    item.kind = display_item::dl_item_type::TEXT_RUN;
    item.x = x;
    item.y = y;
    item.text_content = text;
    item.font_family = font;
    item.font_size = size;
    item.text_color = color;
    if (!m_clip_stack.empty()) {
        item.clip_id = m_clip_stack.back();
        item.is_clipped = true;
    }
    m_items.push_back(item);
}

void display_list::add_image(float x, float y, float w, float h, const std::wstring& url, ID2D1Bitmap1* bitmap) {
    display_item item;
    item.kind = display_item::dl_item_type::DL_BITMAP;
    item.x = x;
    item.y = y;
    item.width = w;
    item.height = h;
    item.image_url = url;
    item.bitmap = bitmap;
    if (!m_clip_stack.empty()) {
        item.clip_id = m_clip_stack.back();
        item.is_clipped = true;
    }
    m_items.push_back(item);
}

void display_list::add_box_shadow(float x, float y, float w, float h, float offset_x, float offset_y, float blur, uint32_t color) {
    display_item item;
    item.kind = display_item::dl_item_type::SHADOW_ITEM;
    item.x = x + offset_x;
    item.y = y + offset_y;
    item.width = w;
    item.height = h;
    item.shadow_offset_x = offset_x;
    item.shadow_offset_y = offset_y;
    item.shadow_blur = blur;
    item.shadow_color = color;
    m_items.push_back(item);
}

void display_list::add_linear_gradient(float x, float y, float w, float h, const std::wstring& gradient_spec) {
    display_item item;
    item.kind = display_item::dl_item_type::LINEAR_GRADIENT;
    item.x = x;
    item.y = y;
    item.width = w;
    item.height = h;
    m_items.push_back(item);
}

void display_list::add_radial_gradient(float x, float y, float w, float h, const std::wstring& gradient_spec) {
    display_item item;
    item.kind = display_item::dl_item_type::RADIAL_GRADIENT;
    item.x = x;
    item.y = y;
    item.width = w;
    item.height = h;
    m_items.push_back(item);
}

void display_list::add_border(float x, float y, float w, float h, float width, uint32_t color, float radius) {
    display_item item;
    item.kind = display_item::dl_item_type::BORDER;
    item.x = x;
    item.y = y;
    item.width = w;
    item.height = h;
    item.border_width = width;
    item.border_color = color;
    item.border_radius = radius;
    m_items.push_back(item);
}

void display_list::push_clip(int clip_id) {
    m_clip_stack.push_back(clip_id);
}

void display_list::pop_clip() {
    if (!m_clip_stack.empty()) {
        m_clip_stack.pop_back();
    }
}

} // namespace hre::render