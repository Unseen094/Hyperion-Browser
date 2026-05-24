#include <hre/svg/svg_renderer.hpp>
#include <cmath>
#include <fstream>

namespace hre::svg {

svg_renderer::svg_renderer() = default;
svg_renderer::~svg_renderer() = default;

bool svg_renderer::load_from_string(const std::wstring& svg_content) {
    return true;
}

bool svg_renderer::load_from_file(const std::wstring& filename) {
    return true;
}

void svg_renderer::render(void* context) {
    m_render_context = context;
    if (m_root) {
        render_element(*m_root, context);
    }
}

svg_element* svg_renderer::get_element_by_id(const std::wstring& id) {
    auto it = m_named_elements.find(id);
    if (it != m_named_elements.end()) {
        return it->second;
    }
    return nullptr;
}

void svg_renderer::render_element(const svg_element& elem, void* context) {
    switch (elem.type) {
        case svg_element_type::RECT:
            break;
        case svg_element_type::CIRCLE:
            break;
        case svg_element_type::PATH:
            break;
        case svg_element_type::TEXT:
            break;
        case svg_element_type::G:
            for (const auto& child : elem.children) {
                if (child) {
                    render_element(*child, context);
                }
            }
            break;
        case svg_element_type::SVG:
            for (const auto& child : elem.children) {
                if (child) {
                    render_element(*child, context);
                }
            }
            break;
        default:
            break;
    }
}

void svg_renderer::apply_style(const svg_style& style, void* context) {
    if (style.opacity < 1.0f) {
    }
}

void svg_transform::rotate(float angle, float cx, float cy) {
    float rad = angle * 3.14159265f / 180.0f;
    float cos_a = std::cos(rad);
    float sin_a = std::sin(rad);

    svg_transform t;
    t.a = cos_a; t.b = sin_a;
    t.c = -sin_a; t.d = cos_a;
    t.e = cx - cx * cos_a + cy * sin_a;
    t.f = cy - cx * sin_a - cy * cos_a;

    *this = t;
}

const svg_rect* svg_element::as_rect() const {
    if (type == svg_element_type::RECT) {
        return reinterpret_cast<const svg_rect*>(this);
    }
    return nullptr;
}

const svg_circle* svg_element::as_circle() const {
    if (type == svg_element_type::CIRCLE) {
        return reinterpret_cast<const svg_circle*>(this);
    }
    return nullptr;
}

} // namespace hre::svg