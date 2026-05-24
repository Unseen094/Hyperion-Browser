#include <hre/text/variable_fonts.hpp>

namespace hre::text {

void variable_font::set_axis_value(uint32_t tag, float value) {
    m_axis_values[tag] = value;

    if (tag == static_cast<uint32_t>(font_axis_tag::WEIGHT)) {
        m_metrics.weight = value;
    } else if (tag == static_cast<uint32_t>(font_axis_tag::WIDTH)) {
        m_metrics.width = value;
    } else if (tag == static_cast<uint32_t>(font_axis_tag::SLOPE)) {
        m_metrics.slope = value;
    } else if (tag == static_cast<uint32_t>(font_axis_tag::OPTICAL_SIZE)) {
        m_metrics.optical_size = value;
    } else if (tag == static_cast<uint32_t>(font_axis_tag::GRAD)) {
        m_metrics.grad = value;
    }
}

float variable_font::get_axis_value(uint32_t tag) const {
    auto it = m_axis_values.find(tag);
    if (it != m_axis_values.end()) {
        return it->second;
    }

    for (const auto& axis : m_axes) {
        if (axis.tag == tag) {
            return axis.default_value;
        }
    }

    return 0.0f;
}

bool variable_font::has_axis(uint32_t tag) const {
    for (const auto& axis : m_axes) {
        if (axis.tag == tag) {
            return true;
        }
    }
    return false;
}

const font_axis* variable_font::get_axis(uint32_t tag) const {
    for (const auto& axis : m_axes) {
        if (axis.tag == tag) {
            return &axis;
        }
    }
    return nullptr;
}

void variable_font::set_metrics(const variable_font_metrics& metrics) {
    m_metrics = metrics;
    m_axis_values[static_cast<uint32_t>(font_axis_tag::WEIGHT)] = metrics.weight;
    m_axis_values[static_cast<uint32_t>(font_axis_tag::WIDTH)] = metrics.width;
    m_axis_values[static_cast<uint32_t>(font_axis_tag::SLOPE)] = metrics.slope;
    m_axis_values[static_cast<uint32_t>(font_axis_tag::OPTICAL_SIZE)] = metrics.optical_size;
    m_axis_values[static_cast<uint32_t>(font_axis_tag::GRAD)] = metrics.grad;
}

void variable_font::reset_to_defaults() {
    m_axis_values.clear();
    m_metrics = variable_font_metrics{};
}

} // namespace hre::text