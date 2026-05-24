#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace hre::text {

enum class font_axis_tag : uint32_t {
    WEIGHT = 0x77676974,
    WIDTH = 0x77647468,
    SLANT = 0x736C746E,
    ITALIC = 0x69746163,
    OPTICAL_SIZE = 0x6F707369,
    SLOPE = 0x736C6F70,
    GRAD = 0x67726164,
    ASCENT = 0x6173636E,
    DESCENT = 0x64657363,
    LINE_GAP = 0x6C696E67
};

struct font_axis {
    uint32_t tag;
    float min_value;
    float max_value;
    float default_value;
    float current_value;
    std::wstring name;
};

struct variable_font_metrics {
    float weight = 400.0f;
    float width = 100.0f;
    float slope = 0.0f;
    float optical_size = 12.0f;
    float grad = 0.0f;
};

class variable_font {
public:
    variable_font() = default;

    void set_axis_value(uint32_t tag, float value);
    float get_axis_value(uint32_t tag) const;

    bool has_axis(uint32_t tag) const;
    const font_axis* get_axis(uint32_t tag) const;

    const std::vector<font_axis>& axes() const { return m_axes; }

    variable_font_metrics get_metrics() const { return m_metrics; }
    void set_metrics(const variable_font_metrics& metrics);

    void reset_to_defaults();

    static uint32_t make_tag(char a, char b, char c, char d) {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(c) << 8) |
               static_cast<uint32_t>(d);
    }

private:
    std::vector<font_axis> m_axes;
    variable_font_metrics m_metrics;
    std::map<uint32_t, float> m_axis_values;
};

} // namespace hre::text