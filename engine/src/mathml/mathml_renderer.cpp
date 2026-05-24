#include <hre/mathml/mathml_renderer.hpp>

namespace hre::mathml {

mathml_renderer::mathml_renderer() = default;
mathml_renderer::~mathml_renderer() = default;

bool mathml_renderer::parse_from_string(const std::wstring& mathml_content) {
    return true;
}

void mathml_renderer::render(void* context) {
    m_render_context = context;
    if (m_root) {
        m_root_metrics = layout_element(*m_root);
        render_element(*m_root, context);
    }
}

math_layout_box mathml_renderer::layout_element(const math_element& elem) {
    math_layout_box box;

    switch (elem.type) {
        case math_element_type::MI:
        case math_element_type::MN:
        case math_element_type::MO:
            box.width = static_cast<float>(elem.text_content.length()) * 8.0f;
            box.ascent = 10.0f;
            box.descent = 4.0f;
            if (elem.type == math_element_type::MO) {
                box.italic_correction = 2.0f;
            }
            break;

        case math_element_type::MROW:
        case math_element_type::MRow: {
            float total_width = 0;
            float max_ascent = 0;
            float max_descent = 0;

            for (const auto& child : elem.children) {
                math_layout_box child_box = layout_element(*child);
                total_width += child_box.width;
                max_ascent = std::max(max_ascent, child_box.ascent);
                max_descent = std::max(max_descent, child_box.descent);
            }

            box.width = total_width;
            box.ascent = max_ascent;
            box.descent = max_descent;
            break;
        }

        case math_element_type::MSUB:
        case math_element_type::MSUPER:
        case math_element_type::MSDOWN: {
            if (elem.children.size() >= 2) {
                math_layout_box main_box = layout_element(*elem.children[0]);
                math_layout_box sub_box = layout_element(*elem.children[1]);

                box.ascent = main_box.ascent;
                box.descent = std::max(main_box.descent, sub_box.descent + 2.0f);
                box.width = main_box.width + sub_box.width;
            }
            break;
        }

        case math_element_type::MFRAC: {
            if (elem.children.size() >= 2) {
                math_layout_box num_box = layout_element(*elem.children[0]);
                math_layout_box den_box = layout_element(*elem.children[1]);

                box.ascent = num_box.ascent + 3.0f;
                box.descent = den_box.descent + 3.0f;
                box.width = std::max(num_box.width, den_box.width) + 4.0f;
            }
            break;
        }

        default:
            box.width = 20.0f;
            box.ascent = 10.0f;
            box.descent = 4.0f;
            break;
    }

    return box;
}

void mathml_renderer::render_element(const math_element& elem, void* context) {
    for (const auto& child : elem.children) {
        if (child) {
            render_element(*child, context);
        }
    }
}

void mathml_renderer::apply_italic_correction(math_element& elem) {
}

} // namespace hre::mathml