#pragma once

#include <string>
#include <cstdint>
#include <array>

namespace hre::css {

struct color_value {
    enum class color_space {
        SRGB,
        SRGB_LINEAR,
        DISPLAY_P3,
        OKLCH,
        OKLAB,
        XYZ,
        XYZ_D50,
        HSL,
        HWB,
        LCH,
        LAB
    } space = color_space::SRGB;

    float components[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    float r() const { return components[0]; }
    float g() const { return components[1]; }
    float b() const { return components[2]; }
    float a() const { return components[3]; }

    void set_r(float v) { components[0] = v; }
    void set_g(float v) { components[1] = v; }
    void set_b(float v) { components[2] = v; }
    void set_a(float v) { components[3] = v; }

    uint32_t to_rgba() const;
    static color_value from_rgba(uint32_t rgba);
    static color_value from_hex(const std::wstring& hex);

    color_value convert(color_space target) const;
    color_value to_srgb() const { return convert(color_space::SRGB); }

    static color_value from_hsl(float h, float s, float l, float a = 1.0f);
    static color_value from_oklch(float l, float c, float h, float alpha = 1.0f);
    static color_value from_lab(float l, float a, float b, float alpha = 1.0f);

    static color_value parse(const std::wstring& str);
    std::wstring to_string() const;

    bool is_valid() const;
};

color_value mix(const color_value& color1, const color_value& color2, float t);

float srgb_transfer_function(float x);
float inverse_srgb_transfer_function(float x);

color_value convert_linear_srgb_to_oklab(const color_value& color);
color_value convert_oklab_to_linear_srgb(const color_value& color);
color_value convert_oklch_to_oklab(const color_value& color);
color_value convert_oklab_to_oklch(const color_value& color);

} // namespace hre::css