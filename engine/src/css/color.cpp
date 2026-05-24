#include <hre/css/color.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace hre::css {

uint32_t color_value::to_rgba() const {
    color_value srgb = to_srgb();

    auto clamp = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };

    uint8_t r = static_cast<uint8_t>(clamp(srgb.components[0]) * 255.0f);
    uint8_t g = static_cast<uint8_t>(clamp(srgb.components[1]) * 255.0f);
    uint8_t b = static_cast<uint8_t>(clamp(srgb.components[2]) * 255.0f);
    uint8_t a = static_cast<uint8_t>(clamp(srgb.components[3]) * 255.0f);

    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

color_value color_value::from_rgba(uint32_t rgba) {
    color_value result;
    result.space = color_space::SRGB;
    result.components[0] = ((rgba >> 16) & 0xFF) / 255.0f;
    result.components[1] = ((rgba >> 8) & 0xFF) / 255.0f;
    result.components[2] = (rgba & 0xFF) / 255.0f;
    result.components[3] = ((rgba >> 24) & 0xFF) / 255.0f;
    return result;
}

color_value color_value::from_hex(const std::wstring& hex) {
    std::string narrow(hex.begin(), hex.end());
    unsigned int rgba = std::stoul(narrow, nullptr, 16);

    if (narrow.length() == 7) {
        rgba = (0xFF << 24) | rgba;
    }

    return from_rgba(rgba);
}

float srgb_transfer_function(float x) {
    if (x <= 0.0031308f) {
        return x * 12.92f;
    }
    return 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

float inverse_srgb_transfer_function(float x) {
    if (x <= 0.04045f) {
        return x / 12.92f;
    }
    return std::pow((x + 0.055f) / 1.055f, 2.4f);
}

color_value color_value::from_hsl(float h, float s, float l, float a) {
    color_value result;
    result.space = color_space::HSL;

    float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;

    float r, g, b;
    if (h < 60.0f) { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    result.components[0] = r + m;
    result.components[1] = g + m;
    result.components[2] = b + m;
    result.components[3] = a;

    return result;
}

color_value color_value::from_oklch(float l, float c, float h, float alpha) {
    color_value result;
    result.space = color_space::OKLCH;
    result.components[0] = l;
    result.components[1] = c;
    result.components[2] = h * 3.14159265f / 180.0f;
    result.components[3] = alpha;
    return result;
}

color_value color_value::from_lab(float l, float a, float b, float alpha) {
    color_value result;
    result.space = color_space::LAB;
    result.components[0] = l;
    result.components[1] = a;
    result.components[2] = b;
    result.components[3] = alpha;
    return result;
}

color_value color_value::parse(const std::wstring& str) {
    if (str.empty()) return color_value{};

    if (str[0] == L'#') {
        return from_hex(str);
    }

    if (str.substr(0, 4) == L"oklch") {
        float l = 0, c = 0, h = 0, a = 1;
        return from_oklch(l, c, h, a);
    }

    if (str.substr(0, 3) == L"rgb" || str.substr(0, 4) == L"rgba") {
        return color_value{};
    }

    return color_value{};
}

std::wstring color_value::to_string() const {
    std::wostringstream oss;

    if (space == color_space::SRGB) {
        oss << L"rgb("
            << static_cast<int>(components[0] * 255) << L", "
            << static_cast<int>(components[1] * 255) << L", "
            << static_cast<int>(components[2] * 255) << L")";
    }

    return oss.str();
}

bool color_value::is_valid() const {
    for (int i = 0; i < 4; ++i) {
        if (components[i] < 0.0f || components[i] > 1.0f) {
            return false;
        }
    }
    return true;
}

color_value color_value::convert(color_space target) const {
    color_value result = *this;

    if (space == target) {
        return result;
    }

    if (space == color_space::OKLCH && target == color_space::SRGB) {
        color_value lab = convert(color_space::LAB);
        return lab.convert(color_space::SRGB);
    }

    if (space == color_space::SRGB && target == color_space::OKLCH) {
        color_value oklab = convert(color_space::OKLAB);
        return oklab.convert(color_space::OKLCH);
    }

    result.space = target;
    return result;
}

color_value mix(const color_value& color1, const color_value& color2, float t) {
    color_value c1 = color1.to_srgb();
    color_value c2 = color2.to_srgb();

    color_value result;
    for (int i = 0; i < 4; ++i) {
        result.components[i] = c1.components[i] * (1.0f - t) + c2.components[i] * t;
    }
    result.space = color_value::color_space::SRGB;
    return result;
}

color_value convert_linear_srgb_to_oklab(const color_value& color) {
    float r = color.components[0];
    float g = color.components[1];
    float b = color.components[2];

    float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
    float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
    float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;

    float l_ = std::cbrt(l);
    float m_ = std::cbrt(m);
    float s_ = std::cbrt(s);

    color_value result;
    result.space = color_value::color_space::OKLAB;
    result.components[0] = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
    result.components[1] = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
    result.components[2] = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
    result.components[3] = color.components[3];

    return result;
}

color_value convert_oklab_to_linear_srgb(const color_value& color) {
    float l_ = color.components[0] + 0.3963377774f * color.components[1] + 0.2158037573f * color.components[2];
    float m_ = color.components[0] - 0.1055613458f * color.components[1] - 0.0638541728f * color.components[2];
    float s_ = color.components[0] - 0.0894841775f * color.components[1] - 1.2914855480f * color.components[2];

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    color_value result;
    result.space = color_value::color_space::SRGB_LINEAR;
    result.components[0] = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    result.components[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    result.components[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    result.components[3] = color.components[3];

    return result;
}

color_value convert_oklch_to_oklab(const color_value& color) {
    color_value result;
    result.space = color_value::color_space::OKLAB;
    result.components[0] = color.components[0];
    result.components[1] = color.components[1] * std::cos(color.components[2]);
    result.components[2] = color.components[1] * std::sin(color.components[2]);
    result.components[3] = color.components[3];
    return result;
}

color_value convert_oklab_to_oklch(const color_value& color) {
    color_value result;
    result.space = color_value::color_space::OKLCH;
    result.components[0] = color.components[0];
    result.components[1] = std::sqrt(color.components[1] * color.components[1] + color.components[2] * color.components[2]);
    result.components[2] = std::atan2(color.components[2], color.components[1]);
    result.components[3] = color.components[3];
    return result;
}

} // namespace hre::css