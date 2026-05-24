#pragma once

#include <hre/layout/layout_engine.hpp>
#include <hre/render/render_command.hpp>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <wrl/client.h>
#include <d2d1_3.h>

namespace hre::render {

struct display_item {
    enum class dl_item_type : uint32_t {
        SOLID_RECT = 1,
        ROUNDED_RECT = 2,
        TEXT_RUN = 3,
        DL_BITMAP = 4,
        SHADOW_ITEM = 5,
        LINEAR_GRADIENT = 6,
        RADIAL_GRADIENT = 7,
        BORDER = 8,
        CLIP = 9
    };
    dl_item_type kind = dl_item_type::SOLID_RECT;

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    uint32_t bg_color = 0;
    float border_radius = 0.0f;
    float border_width = 0.0f;
    uint32_t border_color = 0;

    float shadow_offset_x = 0.0f;
    float shadow_offset_y = 0.0f;
    float shadow_blur = 0.0f;
    uint32_t shadow_color = 0;

    std::wstring text_content;
    std::wstring font_family;
    float font_size = 14.0f;
    uint32_t text_color = 0xFF000000;

    std::wstring image_url;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;

    int clip_id = -1;
    bool is_clipped = false;
};

class display_list {
public:
    display_list() = default;

    void build_from_layout(const layout::LayoutNode& root);

    void add_solid_rect(float x, float y, float w, float h, uint32_t color);
    void add_rounded_rect(float x, float y, float w, float h, float radius, uint32_t color);
    void add_text_run(float x, float y, const std::wstring& text, const std::wstring& font, float size, uint32_t color);
    void add_image(float x, float y, float w, float h, const std::wstring& url, ID2D1Bitmap1* bitmap);
    void add_box_shadow(float x, float y, float w, float h, float offset_x, float offset_y, float blur, uint32_t color);
    void add_linear_gradient(float x, float y, float w, float h, const std::wstring& gradient_spec);
    void add_radial_gradient(float x, float y, float w, float h, const std::wstring& gradient_spec);
    void add_border(float x, float y, float w, float h, float width, uint32_t color, float radius = 0.0f);

    void push_clip(int clip_id);
    void pop_clip();

    const std::vector<display_item>& items() const { return m_items; }
    size_t item_count() const { return m_items.size(); }

    void clear() { m_items.clear(); m_clip_stack.clear(); }

private:
    void build_recursive(const layout::LayoutNode& node);

    std::vector<display_item> m_items;
    std::vector<int> m_clip_stack;
    int m_next_clip_id = 0;
};

} // namespace hre::render