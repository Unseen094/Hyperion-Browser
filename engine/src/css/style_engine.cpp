#include "hre/css/style_engine.hpp"
#include "hre/css/selector_ext.hpp"
#include <algorithm>
#include <sstream>
#include <regex>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <cwctype>

namespace hre::css {

static std::wstring trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::wstring> split_comma_list(const std::wstring& list) {
    std::vector<std::wstring> result;
    std::wstring current;
    int depth = 0;
    for (wchar_t c : list) {
        if (c == L'(') depth++;
        else if (c == L')') { if (depth > 0) depth--; }
        if (c == L',' && depth == 0) {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) result.push_back(trim(current));
    return result;
}

static std::vector<std::wstring> split_whitespace(const std::wstring& s) {
    std::wistringstream ss(s);
    std::wstring token;
    std::vector<std::wstring> result;
    while (ss >> token) result.push_back(token);
    return result;
}

// Extract content inside parentheses
static std::wstring extract_paren_content(const std::wstring& s, size_t open_paren) {
    if (open_paren == std::wstring::npos) return L"";
    int depth = 1;
    size_t pos = open_paren + 1;
    while (pos < s.size() && depth > 0) {
        if (s[pos] == L'(') depth++;
        if (s[pos] == L')') depth--;
        pos++;
    }
    return s.substr(open_paren + 1, pos - open_paren - 2);
}

// --- CssValue ---

static CssUnit parse_unit(const std::wstring& s) {
    if (s == L"px") return CssUnit::Pixel;
    if (s == L"%") return CssUnit::Percent;
    if (s == L"em") return CssUnit::Em;
    if (s == L"rem") return CssUnit::Rem;
    if (s == L"vw") return CssUnit::Vw;
    if (s == L"vh") return CssUnit::Vh;
    if (s == L"vmin") return CssUnit::Vmin;
    if (s == L"vmax") return CssUnit::Vmax;
    if (s == L"ch") return CssUnit::Ch;
    if (s == L"ex") return CssUnit::Ex;
    if (s == L"cm") return CssUnit::Cm;
    if (s == L"mm") return CssUnit::Mm;
    if (s == L"in") return CssUnit::In;
    if (s == L"pt") return CssUnit::Pt;
    if (s == L"pc") return CssUnit::Pc;
    if (s == L"deg") return CssUnit::Deg;
    if (s == L"rad") return CssUnit::Rad;
    if (s == L"grad") return CssUnit::Grad;
    if (s == L"turn") return CssUnit::Turn;
    if (s == L"s") return CssUnit::S;
    if (s == L"ms") return CssUnit::Ms;
    if (s == L"Hz" || s == L"hz") return CssUnit::Hz;
    if (s == L"kHz" || s == L"khz") return CssUnit::Khz;
    if (s == L"dpi") return CssUnit::Dpi;
    if (s == L"dpcm") return CssUnit::Dpcm;
    if (s == L"dppx") return CssUnit::Dppx;
    if (s == L"fr") return CssUnit::Fr;
    return CssUnit::None;
}

CssValue CssValue::parse(const std::wstring& str) {
    CssValue result;
    result.raw = str;
    std::wstring s = trim(str);
    if (s.empty()) return result;

    // Keywords
    if (s == L"auto") { result.unit = CssUnit::Auto; return result; }
    if (s == L"inherit") { result.unit = CssUnit::Inherit; return result; }
    if (s == L"initial") { result.unit = CssUnit::Initial; return result; }
    if (s == L"unset") { result.unit = CssUnit::Unset; return result; }
    if (s == L"revert") { result.unit = CssUnit::Revert; return result; }
    if (s == L"none") { result.unit = CssUnit::None; result.number = 0; return result; }
    if (s == L"normal") { result.unit = CssUnit::None; return result; }

    // Try numeric parsing
    size_t pos = 0;
    bool neg = false;
    if (pos < s.size() && s[pos] == L'-') { neg = true; pos++; }
    else if (pos < s.size() && s[pos] == L'+') pos++;
    while (pos < s.size() && (s[pos] == L'.' || (s[pos] >= L'0' && s[pos] <= L'9'))) pos++;

    if (pos > (neg ? 1 : 0)) {
        try { result.number = std::stod(s.substr(0, pos)); } catch (...) { return result; }
        std::wstring unit_str = s.substr(pos);
        result.unit = parse_unit(unit_str);
        if (result.unit == CssUnit::None) {
            result.string_value = s;
        } else if (result.unit == CssUnit::Percent) {
            result.number /= 100.0;
        }
    } else {
        result.string_value = s;
    }
    return result;
}

double CssValue::to_pixels(double base_size, double viewport_w, double viewport_h) const {
    switch (unit) {
        case CssUnit::Pixel: return number;
        case CssUnit::Percent: return number;
        case CssUnit::Em: return number * base_size;
        case CssUnit::Rem: return number * 16.0;
        case CssUnit::Vw: return number * viewport_w / 100.0;
        case CssUnit::Vh: return number * viewport_h / 100.0;
        case CssUnit::Vmin: return number * std::min(viewport_w, viewport_h) / 100.0;
        case CssUnit::Vmax: return number * std::max(viewport_w, viewport_h) / 100.0;
        case CssUnit::In: return number * 96.0;
        case CssUnit::Cm: return number * 96.0 / 2.54;
        case CssUnit::Mm: return number * 96.0 / 25.4;
        case CssUnit::Pt: return number * 96.0 / 72.0;
        case CssUnit::Pc: return number * 96.0 / 6.0;
        case CssUnit::Auto: return 0.0;
        case CssUnit::Inherit: case CssUnit::Initial: return 0.0;
        default: return number;
    }
}

double CssValue::to_degrees() const {
    switch (unit) {
        case CssUnit::Deg: return number;
        case CssUnit::Rad: return number * 180.0 / 3.14159265358979323846;
        case CssUnit::Grad: return number * 0.9;
        case CssUnit::Turn: return number * 360.0;
        default: return number;
    }
}

double CssValue::to_seconds() const {
    if (unit == CssUnit::Ms) return number / 1000.0;
    return number; // assume seconds
}

// --- CssColor ---

static double parse_hex_byte(const std::wstring& s, size_t offset) {
    auto hex_val = [](wchar_t c) -> int {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return 10 + (c - L'a');
        if (c >= L'A' && c <= L'F') return 10 + (c - L'A');
        return 0;
    };
    if (s.size() - offset >= 2) return hex_val(s[offset]) * 16 + hex_val(s[offset + 1]);
    if (s.size() - offset >= 1) return hex_val(s[offset]) * 17;
    return 0;
}

static std::tuple<double,double,double,double> parse_hsl(double h, double s, double l, double a) {
    h = fmod(h, 360.0); if (h < 0) h += 360.0;
    s = std::clamp(s, 0.0, 1.0);
    l = std::clamp(l, 0.0, 1.0);
    auto hue_to_rgb = [](double p, double q, double t) -> double {
        if (t < 0) t += 1.0;
        if (t > 1) t -= 1.0;
        if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
        if (t < 1.0/2.0) return q;
        if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
        return p;
    };
    double q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    double p = 2.0 * l - q;
    double r = hue_to_rgb(p, q, h / 360.0 + 1.0/3.0);
    double g = hue_to_rgb(p, q, h / 360.0);
    double b_val = hue_to_rgb(p, q, h / 360.0 - 1.0/3.0);
    return {r, g, b_val, a};
}

static std::tuple<double,double,double,double> parse_lab(double L, double a, double b_val, double alpha) {
    // Simplified Lab→XYZ→linear sRGB
    double fy = (L + 16.0) / 116.0;
    double fx = a / 500.0 + fy;
    double fz = fy - b_val / 200.0;
    double x = (fx > 0.206897) ? fx * fx * fx : (fx - 16.0/116.0) / 7.787;
    double y_lab = (fy > 0.206897) ? fy * fy * fy : (fy - 16.0/116.0) / 7.787;
    double z = (fz > 0.206897) ? fz * fz * fz : (fz - 16.0/116.0) / 7.787;
    x *= 0.9642; y_lab *= 1.0; z *= 0.8249;
    // XYZ to linear sRGB
    double r =  3.2406*x - 1.5372*y_lab - 0.4986*z;
    double g = -0.9689*x + 1.8758*y_lab + 0.0415*z;
    double b =  0.0557*x - 0.2040*y_lab + 1.0570*z;
    auto srgb_gamma = [](double c) -> double {
        return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0/2.4) - 0.055;
    };
    return {srgb_gamma(std::clamp(r,0.0,1.0)), srgb_gamma(std::clamp(g,0.0,1.0)), srgb_gamma(std::clamp(b,0.0,1.0)), alpha};
}

static std::tuple<double,double,double,double> parse_oklab(double L, double a, double b_val, double alpha) {
    // Simplified OKLab→linear sRGB
    double l_ = L + 0.3963377774*a + 0.2158037573*b_val;
    double m_ = L - 0.1055613458*a - 0.0638541728*b_val;
    double s_ = L - 0.0894841775*a - 1.2914855480*b_val;
    double l = l_ * l_ * l_;
    double m = m_ * m_ * m_;
    double s_ch = s_ * s_ * s_;
    double r =  4.0767416621*l - 3.3077115913*m + 0.2309699292*s_ch;
    double g = -1.2684380046*l + 2.6097574011*m - 0.3413193965*s_ch;
    double b_val2 = -0.0041960863*l - 0.7034186147*m + 1.7076147010*s_ch;
    auto srgb_gamma = [](double c) -> double {
        double abs_c = std::abs(c);
        double sign = c >= 0 ? 1.0 : -1.0;
        return sign * (abs_c <= 0.0031308 ? 12.92 * abs_c : 1.055 * std::pow(abs_c, 1.0/2.4) - 0.055);
    };
    return {srgb_gamma(std::clamp(r,0.0,1.0)), srgb_gamma(std::clamp(g,0.0,1.0)), srgb_gamma(std::clamp(b_val2,0.0,1.0)), alpha};
}

CssColor CssColor::parse(const std::wstring& str) {
    CssColor result;
    std::wstring s = trim(str);
    if (s.empty()) return result;

    if (s == L"transparent") { result.is_transparent = true; result.value = std::tuple<double,double,double,double>{0,0,0,0}; return result; }
    if (s == L"currentColor") { result.is_current_color = true; result.is_transparent = false; return result; }

    auto parse_nums = [](const std::wstring& inner, bool expect_pct) -> std::vector<double> {
        std::vector<double> nums;
        std::wstring current;
        for (wchar_t c : inner) {
            if (c == L',' || c == L' ' || c == L'/') {
                if (!current.empty()) {
                    double val = std::stod(current);
                    if (expect_pct && current.find(L'%') != std::wstring::npos) val /= 100.0;
                    nums.push_back(val);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            double val = std::stod(current);
            nums.push_back(val);
        }
        return nums;
    };

    // Hex colors
    if (s[0] == L'#') {
        std::wstring hex = s.substr(1);
        if (hex.size() == 3) {
            double r = parse_hex_byte(hex, 0) / 255.0;
            double g = parse_hex_byte(hex, 1) / 255.0;
            double b = parse_hex_byte(hex, 2) / 255.0;
            result.value = std::tuple<double,double,double,double>{r, g, b, 1.0};
        } else if (hex.size() == 4) {
            double r = parse_hex_byte(hex, 0) / 255.0;
            double g = parse_hex_byte(hex, 1) / 255.0;
            double b = parse_hex_byte(hex, 2) / 255.0;
            double a = parse_hex_byte(hex, 3) / 255.0;
            result.value = std::tuple<double,double,double,double>{r, g, b, a};
        } else if (hex.size() >= 6) {
            double r = parse_hex_byte(hex, 0) / 255.0;
            double g = parse_hex_byte(hex, 2) / 255.0;
            double b = parse_hex_byte(hex, 4) / 255.0;
            double a = 1.0;
            if (hex.size() >= 8) a = parse_hex_byte(hex, 6) / 255.0;
            result.value = std::tuple<double,double,double,double>{r, g, b, a};
        }
        result.is_transparent = false;
        return result;
    }

    // Named colors
    static const std::unordered_map<std::wstring, std::tuple<double,double,double>> named = {
        {L"black", {0,0,0}}, {L"white", {1,1,1}}, {L"red", {1,0,0}}, {L"green", {0,0.5,0}},
        {L"blue", {0,0,1}}, {L"yellow", {1,1,0}}, {L"orange", {1,0.647,0}}, {L"purple", {0.5,0,0.5}},
        {L"gray", {0.5,0.5,0.5}}, {L"grey", {0.5,0.5,0.5}}, {L"silver", {0.753,0.753,0.753}},
        {L"navy", {0,0,0.5}}, {L"teal", {0,0.5,0.5}}, {L"aqua", {0,1,1}}, {L"fuchsia", {1,0,1}},
        {L"maroon", {0.5,0,0}}, {L"olive", {0.5,0.5,0}}, {L"lime", {0,1,0}},
        {L"darkgray", {0.663,0.663,0.663}}, {L"dimgray", {0.412,0.412,0.412}},
        {L"lightgray", {0.827,0.827,0.827}}, {L"coral", {1,0.498,0.314}},
        {L"crimson", {0.863,0.078,0.235}}, {L"cyan", {0,1,1}}, {L"gold", {1,0.843,0}},
        {L"indigo", {0.294,0,0.51}}, {L"ivory", {1,1,0.941}}, {L"khaki", {0.941,0.902,0.549}},
        {L"lavender", {0.902,0.902,0.98}}, {L"magenta", {1,0,1}}, {L"pink", {1,0.753,0.796}},
        {L"plum", {0.867,0.627,0.867}}, {L"salmon", {0.98,0.5,0.447}},
        {L"tan", {0.824,0.706,0.549}}, {L"tomato", {1,0.388,0.278}},
        {L"violet", {0.933,0.51,0.933}}, {L"wheat", {0.961,0.871,0.702}},
        {L"aliceblue", {0.941,0.973,1}}, {L"antiquewhite", {0.98,0.922,0.843}},
        {L"aquamarine", {0.498,1,0.831}}, {L"azure", {0.941,1,1}},
        {L"beige", {0.961,0.961,0.863}}, {L"bisque", {1,0.894,0.769}},
        {L"blanchedalmond", {1,0.922,0.804}}, {L"blueviolet", {0.541,0.169,0.886}},
        {L"brown", {0.647,0.165,0.165}}, {L"burlywood", {0.871,0.722,0.529}},
        {L"cadetblue", {0.373,0.62,0.627}}, {L"chartreuse", {0.498,1,0}},
        {L"chocolate", {0.824,0.412,0.118}}, {L"cornflowerblue", {0.392,0.584,0.929}},
        {L"cornsilk", {1,0.973,0.863}}, {L"darkblue", {0,0,0.545}},
        {L"darkcyan", {0,0.545,0.545}}, {L"darkgoldenrod", {0.722,0.525,0.043}},
        {L"darkgreen", {0,0.392,0}}, {L"darkkhaki", {0.741,0.718,0.42}},
        {L"darkmagenta", {0.545,0,0.545}}, {L"darkolivegreen", {0.333,0.42,0.184}},
        {L"darkorange", {1,0.549,0}}, {L"darkorchid", {0.6,0.196,0.8}},
        {L"darkred", {0.545,0,0}}, {L"darksalmon", {0.914,0.588,0.478}},
        {L"darkseagreen", {0.561,0.737,0.561}}, {L"darkslateblue", {0.282,0.239,0.545}},
        {L"darkslategray", {0.184,0.31,0.31}}, {L"darkturquoise", {0,0.808,0.82}},
        {L"darkviolet", {0.58,0,0.827}}, {L"deeppink", {1,0.078,0.576}},
        {L"deepskyblue", {0,0.749,1}}, {L"dodgerblue", {0.118,0.565,1}},
        {L"firebrick", {0.698,0.133,0.133}}, {L"floralwhite", {1,0.98,0.941}},
        {L"forestgreen", {0.133,0.545,0.133}}, {L"gainsboro", {0.863,0.863,0.863}},
        {L"ghostwhite", {0.973,0.973,1}}, {L"goldenrod", {0.855,0.647,0.125}},
        {L"greenyellow", {0.678,1,0.184}}, {L"honeydew", {0.941,1,0.941}},
        {L"hotpink", {1,0.412,0.706}}, {L"indianred", {0.804,0.361,0.361}},
        {L"lawngreen", {0.486,0.988,0}}, {L"lemonchiffon", {1,0.98,0.804}},
        {L"lightblue", {0.678,0.847,0.902}}, {L"lightcoral", {0.941,0.502,0.502}},
        {L"lightcyan", {0.878,1,1}}, {L"lightgoldenrodyellow", {0.98,0.98,0.824}},
        {L"lightgreen", {0.565,0.933,0.565}}, {L"lightpink", {1,0.714,0.757}},
        {L"lightsalmon", {1,0.627,0.478}}, {L"lightseagreen", {0.125,0.698,0.667}},
        {L"lightskyblue", {0.529,0.808,0.98}}, {L"lightslategray", {0.467,0.533,0.6}},
        {L"lightsteelblue", {0.69,0.769,0.871}}, {L"lightyellow", {1,1,0.878}},
        {L"limegreen", {0.196,0.804,0.196}}, {L"linen", {0.98,0.941,0.902}},
        {L"mediumaquamarine", {0.4,0.804,0.667}}, {L"mediumblue", {0,0,0.804}},
        {L"mediumorchid", {0.729,0.333,0.827}}, {L"mediumpurple", {0.576,0.439,0.859}},
        {L"mediumseagreen", {0.235,0.702,0.443}}, {L"mediumslateblue", {0.482,0.408,0.933}},
        {L"mediumspringgreen", {0,0.98,0.604}}, {L"mediumturquoise", {0.282,0.82,0.8}},
        {L"mediumvioletred", {0.78,0.082,0.522}}, {L"midnightblue", {0.098,0.098,0.439}},
        {L"mintcream", {0.961,1,0.98}}, {L"mistyrose", {1,0.894,0.882}},
        {L"moccasin", {1,0.894,0.71}}, {L"navajowhite", {1,0.871,0.678}},
        {L"oldlace", {0.992,0.961,0.902}}, {L"olivedrab", {0.42,0.557,0.137}},
        {L"orangered", {1,0.271,0}}, {L"orchid", {0.855,0.439,0.839}},
        {L"palegoldenrod", {0.933,0.91,0.667}}, {L"palegreen", {0.596,0.984,0.596}},
        {L"paleturquoise", {0.686,0.933,0.933}}, {L"palevioletred", {0.859,0.439,0.576}},
        {L"papayawhip", {1,0.937,0.835}}, {L"peachpuff", {1,0.855,0.725}},
        {L"peru", {0.804,0.522,0.247}}, {L"powderblue", {0.69,0.878,0.902}},
        {L"rosybrown", {0.737,0.561,0.561}}, {L"royalblue", {0.255,0.412,0.882}},
        {L"saddlebrown", {0.545,0.271,0.075}}, {L"sandybrown", {0.957,0.643,0.376}},
        {L"seagreen", {0.18,0.545,0.341}}, {L"seashell", {1,0.961,0.933}},
        {L"sienna", {0.627,0.322,0.176}}, {L"skyblue", {0.529,0.808,0.922}},
        {L"slateblue", {0.416,0.353,0.804}}, {L"slategray", {0.439,0.5,0.565}},
        {L"snow", {1,0.98,0.98}}, {L"springgreen", {0,1,0.498}},
        {L"steelblue", {0.275,0.51,0.706}}, {L"thistle", {0.847,0.749,0.847}},
        {L"turquoise", {0.251,0.878,0.816}}, {L"wheat", {0.961,0.871,0.702}},
        {L"whitesmoke", {0.961,0.961,0.961}}, {L"yellowgreen", {0.604,0.804,0.196}}
    };
    auto it = named.find(s);
    if (it != named.end()) {
        auto [r, g, b] = it->second;
        result.value = std::tuple<double,double,double,double>{r, g, b, 1.0};
        result.is_transparent = false;
        return result;
    }

    // Functional notations
    size_t paren = s.find(L'(');
    if (paren == std::wstring::npos) return result;
    std::wstring func = s.substr(0, paren);
    std::wstring inner = s.substr(paren + 1, s.size() - paren - 2);

    if (func == L"rgb" || func == L"rgba") {
        auto nums = parse_nums(inner, false);
        if (nums.size() >= 3) {
            double r = nums[0] / 255.0, g = nums[1] / 255.0, b_val = nums[2] / 255.0;
            double a = nums.size() >= 4 ? nums[3] : 1.0;
            result.value = std::tuple<double,double,double,double>{r, g, b_val, a};
            result.is_transparent = false;
        }
    } else if (func == L"hsl" || func == L"hsla") {
        auto nums = parse_nums(inner, false);
        if (nums.size() >= 3) {
            double h = nums[0], s = nums[1] / 100.0, l = nums[2] / 100.0;
            double a = nums.size() >= 4 ? nums[3] : 1.0;
            result.value = parse_hsl(h, s, l, a);
            result.is_transparent = false;
        }
    } else if (func == L"lab") {
        auto nums = parse_nums(inner, false);
        if (nums.size() >= 3) {
            result.value = parse_lab(nums[0], nums[1], nums[2], nums.size() >= 4 ? nums[3] : 1.0);
            result.is_transparent = false;
            result.color_space = L"lab";
        }
    } else if (func == L"lch") {
        auto nums = parse_nums(inner, false);
        if (nums.size() >= 3) {
            double L = nums[0], c = nums[1], h = nums[2];
            double a = c * std::cos(h * 3.14159265 / 180.0);
            double b_val = c * std::sin(h * 3.14159265 / 180.0);
            result.value = parse_lab(L, a, b_val, nums.size() >= 4 ? nums[3] : 1.0);
            result.is_transparent = false;
            result.color_space = L"lch";
        }
    } else if (func == L"oklab") {
        auto nums = parse_nums(inner, false);
        if (nums.size() >= 3) {
            result.value = parse_oklab(nums[0], nums[1], nums[2], nums.size() >= 4 ? nums[3] : 1.0);
            result.is_transparent = false;
            result.color_space = L"oklab";
        }
    } else if (func == L"oklch") {
        auto nums = parse_nums(inner, false);
        if (nums.size() >= 3) {
            double L = nums[0], c = nums[1], h_val = nums[2];
            double a = c * std::cos(h_val * 3.14159265 / 180.0);
            double b_val = c * std::sin(h_val * 3.14159265 / 180.0);
            result.value = parse_oklab(L, a, b_val, nums.size() >= 4 ? nums[3] : 1.0);
            result.is_transparent = false;
            result.color_space = L"oklch";
        }
    } else if (func == L"color-mix") {
        // Simplified color-mix: color-mix(in srgb, color1 pct, color2 pct)
        auto parts = split_comma_list(inner);
        if (parts.size() >= 3) {
            CssColor c1 = parse(parts[1]);
            CssColor c2 = parse(parts[2]);
            auto [r1,g1,b1,a1] = c1.to_rgba();
            auto [r2,g2,b2,a2] = c2.to_rgba();
            double mix = 0.5;
            result.value = std::tuple<double,double,double,double>{
                r1*(1-mix)+r2*mix, g1*(1-mix)+g2*mix, b1*(1-mix)+b2*mix, a1*(1-mix)+a2*mix
            };
            result.is_transparent = false;
        }
    }

    return result;
}

std::tuple<double,double,double,double> CssColor::to_rgba() const {
    if (auto* t = std::get_if<std::tuple<double,double,double,double>>(&value)) return *t;
    return {0,0,0,0};
}

// --- MediaQuery ---

bool MediaQuery::evaluate(float viewport_width, float viewport_height, float dpr) const {
    if (negated) return false;
    if (media_type == L"print") return false;
    if (media_type == L"screen") {}
    if (media_type == L"all" || media_type == L"") {}
    for (const auto& [name, val] : features) {
        if (name == L"width") {
            float w = std::stof(val);
            if (std::abs(viewport_width - w) > 0.5) return false;
        } else if (name == L"min-width") {
            if (viewport_width < std::stof(val)) return false;
        } else if (name == L"max-width") {
            if (viewport_width > std::stof(val)) return false;
        } else if (name == L"height") {
            float h = std::stof(val);
            if (std::abs(viewport_height - h) > 0.5) return false;
        } else if (name == L"min-height") {
            if (viewport_height < std::stof(val)) return false;
        } else if (name == L"max-height") {
            if (viewport_height > std::stof(val)) return false;
        } else if (name == L"resolution" || name == L"min-resolution" || name == L"max-resolution") {
            float res = std::stof(val);
            if (name.find(L"min-") == 0 && dpr * 96 < res) return false;
            else if (name.find(L"max-") == 0 && dpr * 96 > res) return false;
            else if (name == L"resolution" && std::abs(dpr * 96 - res) > 0.5) return false;
        } else if (name == L"orientation") {
            if (val == L"portrait" && viewport_width > viewport_height) return false;
            if (val == L"landscape" && viewport_width < viewport_height) return false;
        } else if (name == L"prefers-color-scheme") {
            if (val == L"dark") return false; // Assume light mode for now
        } else if (name == L"prefers-reduced-motion") {
            if (val == L"no-preference") return true;
            return false;
        }
    }
    return true;
}

// --- SupportsCondition ---

bool SupportsCondition::evaluate() const {
    if (negated) return false;
    for (const auto& decl : declarations) {
        size_t colon = decl.find(L':');
        if (colon == std::wstring::npos) continue;
        std::wstring prop = trim(decl.substr(0, colon));
        // Check if we support this property (approximate: we support most common ones)
        static const std::unordered_set<std::wstring> supported = {
            L"display", L"position", L"color", L"background", L"background-color",
            L"margin", L"padding", L"width", L"height", L"font-size", L"font-family",
            L"flex-direction", L"justify-content", L"align-items", L"flex-wrap",
            L"grid-template-columns", L"grid-template-rows", L"gap", L"transform",
            L"transition", L"animation", L"filter", L"opacity", L"overflow",
            L"text-align", L"text-decoration", L"border", L"border-radius",
            L"box-shadow", L"visibility", L"z-index", L"top", L"left", L"right", L"bottom"
        };
        return supported.count(prop) > 0;
    }
    return true;
}

// --- Parsing ---

static void parse_at_rules(const std::wstring& css, std::vector<ParsedRule>& out_rules,
                           std::map<std::wstring, KeyframesRule>& keyframes,
                           std::vector<FontFace>& font_faces, int& source_counter, bool is_ua) {
    size_t pos = 0;
    while (pos < css.size()) {
        // Skip whitespace
        while (pos < css.size() && (css[pos] == L' ' || css[pos] == L'\t' || css[pos] == L'\n' || css[pos] == L'\r')) pos++;
        if (pos >= css.size()) break;

        // Comments
        if (pos + 1 < css.size() && css[pos] == L'/' && css[pos+1] == L'*') {
            size_t end = css.find(L"*/", pos + 2);
            if (end == std::wstring::npos) break;
            pos = end + 2;
            continue;
        }

        if (css[pos] == L'@') {
            size_t ws = css.find_first_of(L" \t\n\r({", pos);
            if (ws == std::wstring::npos) break;
            std::wstring at_keyword = css.substr(pos + 1, ws - pos - 1);

            if (at_keyword == L"media") {
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) break;
                std::wstring prelude = css.substr(ws, block_start - ws);

                // Parse media query
                auto mq = std::make_shared<MediaQuery>();
                std::wstring mq_text = trim(prelude);
                if (mq_text.find(L"not ") == 0) { mq->negated = true; mq_text = trim(mq_text.substr(4)); }
                if (mq_text.find(L"only ") == 0) mq_text = trim(mq_text.substr(5));
                size_t and_pos = mq_text.find(L" and ");
                if (and_pos == std::wstring::npos) {
                    if (mq_text.find(L'(') == 0) {
                        mq->media_type = L"all";
                    } else {
                        mq->media_type = mq_text;
                    }
                } else {
                    mq->media_type = trim(mq_text.substr(0, and_pos));
                    std::wstring rest = mq_text.substr(and_pos + 5);
                    size_t fpos = 0;
                    while (fpos < rest.size()) {
                        while (fpos < rest.size() && rest[fpos] == L' ') fpos++;
                        if (fpos >= rest.size()) break;
                        if (rest[fpos] == L'(') {
                            size_t close = rest.find(L')', fpos);
                            if (close == std::wstring::npos) break;
                            std::wstring feature = rest.substr(fpos + 1, close - fpos - 1);
                            size_t colon = feature.find(L':');
                            if (colon != std::wstring::npos) {
                                mq->features.push_back({trim(feature.substr(0, colon)), trim(feature.substr(colon + 1))});
                            }
                            fpos = close + 1;
                        } else fpos++;
                    }
                }

                // Parse nested rules
                int depth = 1;
                size_t rpos = block_start + 1;
                size_t rule_start = rpos;
                while (rpos < css.size() && depth > 0) {
                    if (css[rpos] == L'{') depth++;
                    else if (css[rpos] == L'}') depth--;
                    if (depth == 0) {
                        std::wstring nested = css.substr(rule_start, rpos - rule_start);
                        size_t np = 0;
                        while (np < nested.size()) {
                            while (np < nested.size() && (nested[np] == L' ' || nested[np] == L'\t' || nested[np] == L'\n' || nested[np] == L'\r')) np++;
                            if (np >= nested.size()) break;
                            if (np + 1 < nested.size() && nested[np] == L'/' && nested[np+1] == L'*') {
                                size_t ce = nested.find(L"*/", np + 2);
                                np = (ce == std::wstring::npos) ? nested.size() : ce + 2;
                                continue;
                            }
                            size_t ob = nested.find(L'{', np);
                            if (ob == std::wstring::npos) break;
                            std::wstring selectors_str = nested.substr(np, ob - np);
                            size_t cb = nested.find(L'}', ob);
                            if (cb == std::wstring::npos) break;
                            std::wstring block = nested.substr(ob + 1, cb - ob - 1);

                            // Split selectors by comma
                            auto sel_list = split_comma_list(selectors_str);
                            auto decls = split_comma_list(block);

                            for (const auto& sel : sel_list) {
                                ParsedRule rule;
                                rule.selectors.push_back(trim(sel));
                                rule.at_rule_type = AtRuleType::Media;
                                rule.media_query = mq;
                                rule.source_order = source_counter++;

                                // Parse declarations
                                size_t dp = 0;
                                while (dp < block.size()) {
                                    while (dp < block.size() && (block[dp] == L' ' || block[dp] == L'\t' || block[dp] == L'\n' || block[dp] == L'\r')) dp++;
                                    if (dp >= block.size()) break;
                                    size_t colon = block.find(L':', dp);
                                    if (colon == std::wstring::npos) break;
                                    std::wstring prop = trim(block.substr(dp, colon - dp));
                                    size_t semi = block.find(L';', colon + 1);
                                    if (semi == std::wstring::npos) {
                                        std::wstring val = trim(block.substr(colon + 1));
                                        if (!prop.empty()) rule.declarations.push_back({prop, val});
                                        break;
                                    }
                                    std::wstring val = trim(block.substr(colon + 1, semi - colon - 1));
                                    if (!prop.empty()) {
                                        if (val.find(L"!important") != std::wstring::npos) {
                                            val = trim(val.substr(0, val.find(L"!important")));
                                            rule.important = true;
                                        }
                                        rule.declarations.push_back({prop, val});
                                    }
                                    dp = semi + 1;
                                }

                                if (is_ua) out_rules.push_back(rule);
                                else out_rules.push_back(rule);
                            }
                            np = cb + 1;
                        }
                        rpos++;
                    } else rpos++;
                }
                if (depth > 0) break;
                pos = rpos;
            } else if (at_keyword == L"keyframes" || at_keyword == L"-webkit-keyframes") {
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) break;
                std::wstring name = trim(css.substr(ws, block_start - ws));
                KeyframesRule kfr;
                kfr.name = name;

                int depth = 1;
                size_t kpos = block_start + 1;
                while (kpos < css.size() && depth > 0) {
                    if (css[kpos] == L'{') depth++;
                    else if (css[kpos] == L'}') {
                        depth--;
                        if (depth == 1) {
                            // End of keyframe block
                        }
                    }
                    kpos++;
                }
                // Simple parsing: find keyframe blocks
                size_t kf_pos = block_start + 1;
                int kf_depth = 1;
                while (kf_pos < css.size() && kf_depth > 0) {
                    if (css[kf_pos] == L'{') kf_depth++;
                    else if (css[kf_pos] == L'}') kf_depth--;
                    kf_pos++;
                }
                std::wstring kf_body = css.substr(block_start + 1, kf_pos - block_start - 2);
                size_t kp = 0;
                while (kp < kf_body.size()) {
                    while (kp < kf_body.size() && (kf_body[kp] == L' ' || kf_body[kp] == L'\t' || kf_body[kp] == L'\n' || kf_body[kp] == L'\r')) kp++;
                    if (kp >= kf_body.size()) break;
                    size_t ob = kf_body.find(L'{', kp);
                    if (ob == std::wstring::npos) break;
                    std::wstring kf_sel = trim(kf_body.substr(kp, ob - kp));
                    size_t cb = kf_body.find(L'}', ob);
                    if (cb == std::wstring::npos) break;
                    std::wstring kf_block = kf_body.substr(ob + 1, cb - ob - 1);

                    Keyframe kf;
                    auto parts = split_comma_list(kf_sel);
                    for (const auto& p : parts) kf.selectors.push_back(trim(p));

                    size_t dp = 0;
                    while (dp < kf_block.size()) {
                        size_t colon = kf_block.find(L':', dp);
                        if (colon == std::wstring::npos) break;
                        std::wstring prop = trim(kf_block.substr(dp, colon - dp));
                        size_t semi = kf_block.find(L';', colon + 1);
                        if (semi == std::wstring::npos) {
                            kf.declarations[prop] = trim(kf_block.substr(colon + 1));
                            break;
                        }
                        kf.declarations[prop] = trim(kf_block.substr(colon + 1, semi - colon - 1));
                        dp = semi + 1;
                    }
                    kfr.keyframes.push_back(kf);
                    kp = cb + 1;
                }
                keyframes[kfr.name] = kfr;
                pos = kf_pos;
            } else if (at_keyword == L"font-face") {
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) break;
                size_t block_end = css.find(L'}', block_start);
                if (block_end == std::wstring::npos) break;
                std::wstring block = css.substr(block_start + 1, block_end - block_start - 1);
                FontFace ff;
                size_t dp = 0;
                while (dp < block.size()) {
                    size_t colon = block.find(L':', dp);
                    if (colon == std::wstring::npos) break;
                    std::wstring prop = trim(block.substr(dp, colon - dp));
                    size_t semi = block.find(L';', colon + 1);
                    if (semi == std::wstring::npos) {
                        ff.descriptors[prop] = trim(block.substr(colon + 1));
                        break;
                    }
                    ff.descriptors[prop] = trim(block.substr(colon + 1, semi - colon - 1));
                    dp = semi + 1;
                }
                if (ff.descriptors.count(L"font-family")) ff.font_family = ff.descriptors[L"font-family"];
                if (ff.descriptors.count(L"src")) ff.src = ff.descriptors[L"src"];
                if (ff.descriptors.count(L"font-weight")) ff.font_weight = ff.descriptors[L"font-weight"];
                if (ff.descriptors.count(L"font-style")) ff.font_style = ff.descriptors[L"font-style"];
                if (ff.descriptors.count(L"font-stretch")) ff.font_stretch = ff.descriptors[L"font-stretch"];
                if (ff.descriptors.count(L"font-display")) ff.font_display = ff.descriptors[L"font-display"];
                font_faces.push_back(ff);
                pos = block_end + 1;
            } else if (at_keyword == L"import") {
                size_t semi = css.find(L';', ws);
                if (semi == std::wstring::npos) break;
                pos = semi + 1;
            } else if (at_keyword == L"supports") {
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) break;
                std::wstring condition_str = trim(css.substr(ws, block_start - ws));
                auto sc = std::make_shared<SupportsCondition>();
                sc->raw = condition_str;
                // Simplified: just check if condition starts with "not "
                if (condition_str.find(L"not ") == 0) { sc->negated = true; }

                int depth = 1;
                size_t spos = block_start + 1;
                size_t rule_s = spos;
                while (spos < css.size() && depth > 0) {
                    if (css[spos] == L'{') depth++;
                    else if (css[spos] == L'}') depth--;
                    if (depth == 0) {
                        // Parse nested rules into same output
                        std::wstring nested = css.substr(rule_s, spos - rule_s);
                        size_t np = 0;
                        while (np < nested.size()) {
                            while (np < nested.size() && (nested[np] == L' ' || nested[np] == L'\t' || nested[np] == L'\n' || nested[np] == L'\r')) np++;
                            if (np >= nested.size()) break;
                            size_t ob = nested.find(L'{', np);
                            if (ob == std::wstring::npos) break;
                            std::wstring sel_str = nested.substr(np, ob - np);
                            size_t cb = nested.find(L'}', ob);
                            if (cb == std::wstring::npos) break;
                            std::wstring block = nested.substr(ob + 1, cb - ob - 1);

                            auto sel_list = split_comma_list(sel_str);
                            for (const auto& sel : sel_list) {
                                ParsedRule rule;
                                rule.selectors.push_back(trim(sel));
                                rule.at_rule_type = AtRuleType::Supports;
                                rule.supports_condition = sc;
                                rule.source_order = source_counter++;
                                size_t dp = 0;
                                while (dp < block.size()) {
                                    while (dp < block.size() && (block[dp] == L' ' || block[dp] == L'\t' || block[dp] == L'\n' || block[dp] == L'\r')) dp++;
                                    if (dp >= block.size()) break;
                                    size_t colon = block.find(L':', dp);
                                    if (colon == std::wstring::npos) break;
                                    std::wstring prop = trim(block.substr(dp, colon - dp));
                                    size_t semi = block.find(L';', colon + 1);
                                    if (semi == std::wstring::npos) {
                                        std::wstring val = trim(block.substr(colon + 1));
                                        if (!prop.empty()) rule.declarations.push_back({prop, val});
                                        break;
                                    }
                                    std::wstring val = trim(block.substr(colon + 1, semi - colon - 1));
                                    if (!prop.empty()) {
                                        if (val.find(L"!important") != std::wstring::npos) {
                                            val = trim(val.substr(0, val.find(L"!important")));
                                            rule.important = true;
                                        }
                                        rule.declarations.push_back({prop, val});
                                    }
                                    dp = semi + 1;
                                }
                                out_rules.push_back(rule);
                            }
                            np = cb + 1;
                        }
                        spos++;
                    } else spos++;
                }
                if (depth > 0) break;
                pos = spos;
            } else if (at_keyword == L"layer") {
                size_t semi = css.find(L';', ws);
                if (semi != std::wstring::npos) {
                    pos = semi + 1;
                    continue;
                }
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) { pos = ws + 1; continue; }
                int depth = 1;
                size_t lpos = block_start + 1;
                while (lpos < css.size() && depth > 0) {
                    if (css[lpos] == L'{') depth++;
                    else if (css[lpos] == L'}') depth--;
                    lpos++;
                }
                // Parse nested rules into output (layer name tracking omitted for brevity)
                pos = lpos;
            } else if (at_keyword == L"namespace") {
                size_t semi = css.find(L';', ws);
                if (semi == std::wstring::npos) break;
                pos = semi + 1;
            } else if (at_keyword == L"property") {
                size_t semi = css.find(L';', ws);
                if (semi == std::wstring::npos) break;
                pos = semi + 1;
            } else if (at_keyword == L"counter-style") {
                size_t semi = css.find(L';', ws);
                if (semi == std::wstring::npos) {
                    size_t block_start = css.find(L'{', ws);
                    if (block_start == std::wstring::npos) break;
                    size_t block_end = css.find(L'}', block_start);
                    if (block_end == std::wstring::npos) break;
                    pos = block_end + 1;
                } else pos = semi + 1;
            } else if (at_keyword == L"container") {
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) { pos = ws + 1; continue; }
                int depth = 1;
                size_t cpos = block_start + 1;
                while (cpos < css.size() && depth > 0) {
                    if (css[cpos] == L'{') depth++;
                    else if (css[cpos] == L'}') depth--;
                    cpos++;
                }
                pos = cpos;
            } else if (at_keyword == L"scope") {
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) { pos = ws + 1; continue; }
                int depth = 1;
                size_t scpos = block_start + 1;
                while (scpos < css.size() && depth > 0) {
                    if (css[scpos] == L'{') depth++;
                    else if (css[scpos] == L'}') depth--;
                    scpos++;
                }
                pos = scpos;
            } else if (at_keyword == L"page") {
                size_t block_start = css.find(L'{', ws);
                if (block_start == std::wstring::npos) break;
                size_t block_end = css.find(L'}', block_start);
                if (block_end == std::wstring::npos) break;
                pos = block_end + 1;
            } else {
                // Unknown at-rule, skip to next block or semicolon
                size_t semi = css.find(L';', ws);
                size_t block = css.find(L'{', ws);
                if (block != std::wstring::npos && (semi == std::wstring::npos || block < semi)) {
                    int depth = 1;
                    size_t upos = block + 1;
                    while (upos < css.size() && depth > 0) {
                        if (css[upos] == L'{') depth++;
                        else if (css[upos] == L'}') depth--;
                        upos++;
                    }
                    pos = upos;
                } else if (semi != std::wstring::npos) pos = semi + 1;
                else break;
            }
        } else {
            // Regular rule
            size_t brace_open = css.find(L'{', pos);
            if (brace_open == std::wstring::npos) break;
            std::wstring selectors_str = css.substr(pos, brace_open - pos);
            std::wstring selectors_str_trimmed = trim(selectors_str);
            if (selectors_str_trimmed.empty()) { pos = brace_open + 1; continue; }

            size_t brace_close = css.find(L'}', brace_open);
            if (brace_close == std::wstring::npos) break;
            std::wstring block = css.substr(brace_open + 1, brace_close - brace_open - 1);

            auto selector_list = split_comma_list(selectors_str);
            for (const auto& sel : selector_list) {
                std::wstring trimmed_sel = trim(sel);
                if (trimmed_sel.empty()) continue;

                ParsedRule rule;
                rule.selectors.push_back(trimmed_sel);
                rule.source_order = source_counter++;

                size_t dp = 0;
                while (dp < block.size()) {
                    while (dp < block.size() && (block[dp] == L' ' || block[dp] == L'\t' || block[dp] == L'\n' || block[dp] == L'\r')) dp++;
                    if (dp >= block.size()) break;
                    if (dp + 1 < block.size() && block[dp] == L'/' && block[dp+1] == L'*') {
                        size_t ce = block.find(L"*/", dp + 2);
                        dp = (ce == std::wstring::npos) ? block.size() : ce + 2;
                        continue;
                    }
                    size_t colon = block.find(L':', dp);
                    if (colon == std::wstring::npos) break;
                    std::wstring prop = trim(block.substr(dp, colon - dp));
                    size_t semi = block.find(L';', colon + 1);
                    if (semi == std::wstring::npos) {
                        std::wstring val = trim(block.substr(colon + 1));
                        if (!prop.empty()) {
                            bool imp = false;
                            if (val.find(L"!important") != std::wstring::npos) {
                                val = trim(val.substr(0, val.find(L"!important")));
                                imp = true;
                            }
                            rule.declarations.push_back({prop, val});
                            rule.important = imp;
                        }
                        break;
                    }
                    std::wstring val = trim(block.substr(colon + 1, semi - colon - 1));
                    if (!prop.empty()) {
                        bool imp = false;
                        if (val.find(L"!important") != std::wstring::npos) {
                            val = trim(val.substr(0, val.find(L"!important")));
                            imp = true;
                        }
                        rule.declarations.push_back({prop, val});
                        rule.important = imp;
                    }
                    dp = semi + 1;
                }

                out_rules.push_back(rule);
            }
            pos = brace_close + 1;
        }
    }
}

// --- UA Default Styles ---

std::vector<std::pair<std::wstring, std::map<std::wstring, std::wstring>>> get_ua_defaults() {
    return {
        {L"html", {{L"display", L"block"}, {L"color", L"initial"}}},
        {L"head", {{L"display", L"none"}}},
        {L"body", {{L"display", L"block"}, {L"margin", L"8px"}}},
        {L"div", {{L"display", L"block"}}},
        {L"p", {{L"display", L"block"}, {L"margin-top", L"1em"}, {L"margin-bottom", L"1em"}}},
        {L"h1", {{L"display", L"block"}, {L"font-size", L"2em"}, {L"font-weight", L"bold"}, {L"margin", L"0.67em 0"}}},
        {L"h2", {{L"display", L"block"}, {L"font-size", L"1.5em"}, {L"font-weight", L"bold"}, {L"margin", L"0.83em 0"}}},
        {L"h3", {{L"display", L"block"}, {L"font-size", L"1.17em"}, {L"font-weight", L"bold"}, {L"margin", L"1em 0"}}},
        {L"h4", {{L"display", L"block"}, {L"font-size", L"1em"}, {L"font-weight", L"bold"}, {L"margin", L"1.33em 0"}}},
        {L"h5", {{L"display", L"block"}, {L"font-size", L"0.83em"}, {L"font-weight", L"bold"}, {L"margin", L"1.67em 0"}}},
        {L"h6", {{L"display", L"block"}, {L"font-size", L"0.67em"}, {L"font-weight", L"bold"}, {L"margin", L"2.33em 0"}}},
        {L"a", {{L"color", L"#0000EE"}, {L"text-decoration", L"underline"}}},
        {L"strong", {{L"font-weight", L"bold"}}},
        {L"em", {{L"font-style", L"italic"}}},
        {L"u", {{L"text-decoration", L"underline"}}},
        {L"ul", {{L"display", L"block"}, {L"list-style-type", L"disc"}, {L"margin", L"1em 0"}, {L"padding-left", L"40px"}}},
        {L"ol", {{L"display", L"block"}, {L"list-style-type", L"decimal"}, {L"margin", L"1em 0"}, {L"padding-left", L"40px"}}},
        {L"li", {{L"display", L"list-item"}}},
        {L"table", {{L"display", L"table"}}},
        {L"tr", {{L"display", L"table-row"}}},
        {L"td", {{L"display", L"table-cell"}, {L"padding", L"1px"}}},
        {L"th", {{L"display", L"table-cell"}, {L"font-weight", L"bold"}, {L"padding", L"1px"}}},
        {L"img", {{L"display", L"inline-block"}}},
        {L"span", {{L"display", L"inline"}}},
        {L"br", {{L"display", L"inline"}}},
        {L"form", {{L"display", L"block"}}},
        {L"input", {{L"display", L"inline-block"}}},
        {L"button", {{L"display", L"inline-block"}}},
        {L"textarea", {{L"display", L"inline-block"}}},
        {L"select", {{L"display", L"inline-block"}}},
        {L"label", {{L"display", L"inline"}}},
        {L"script", {{L"display", L"none"}}},
        {L"style", {{L"display", L"none"}}},
        {L"link", {{L"display", L"none"}}},
        {L"meta", {{L"display", L"none"}}},
        {L"title", {{L"display", L"none"}}},
        {L"section", {{L"display", L"block"}}},
        {L"article", {{L"display", L"block"}}},
        {L"nav", {{L"display", L"block"}}},
        {L"aside", {{L"display", L"block"}}},
        {L"header", {{L"display", L"block"}}},
        {L"footer", {{L"display", L"block"}}},
        {L"main", {{L"display", L"block"}}},
        {L"figure", {{L"display", L"block"}}},
        {L"figcaption", {{L"display", L"block"}}},
        {L"blockquote", {{L"display", L"block"}, {L"margin", L"1em 40px"}}},
        {L"pre", {{L"display", L"block"}, {L"white-space", L"pre"}}},
        {L"code", {{L"font-family", L"monospace"}}},
        {L"hr", {{L"display", L"block"}, {L"border", L"1px inset"}}},
    };
}

// --- StyleEngine ---

StyleEngine::StyleEngine() {}

void StyleEngine::load_ua_stylesheet() {
    auto ua_defaults = get_ua_defaults();
    for (const auto& [tag, props] : ua_defaults) {
        ParsedRule rule;
        rule.selectors.push_back(tag);
        rule.source_order = m_source_order_counter++;
        rule.origin = CascadeOrigin::UA;
        for (const auto& [prop, val] : props) {
            rule.declarations.push_back({prop, val});
        }
        m_ua_rules.push_back(std::move(rule));
    }
}

void StyleEngine::load_stylesheet(const std::wstring& css) {
    parse_rules(css, m_parsed_rules, false);
}

void StyleEngine::load_raw_rules(const std::vector<std::wstring>& rules) {
    for (const auto& rule : rules) {
        parse_rules(rule, m_parsed_rules, false);
    }
}

void StyleEngine::parse_rules(const std::wstring& css, std::vector<ParsedRule>& out_rules, bool is_ua) {
    parse_at_rules(css, out_rules, m_keyframes, m_font_faces, m_source_order_counter, is_ua);
}

// --- Inherited Properties ---

bool StyleEngine::is_inherited_property(const std::wstring& name) {
    static const std::unordered_set<std::wstring> inherited = {
        L"color", L"font-family", L"font-size", L"font-style", L"font-weight",
        L"font-stretch", L"font-variant", L"letter-spacing", L"line-height",
        L"text-align", L"text-indent", L"text-transform", L"visibility",
        L"white-space", L"word-break", L"word-spacing", L"overflow-wrap",
        L"direction", L"unicode-bidi", L"cursor", L"orphans", L"widows",
        L"text-decoration-color", L"text-decoration-line", L"text-decoration-style",
        L"text-decoration-thickness", L"text-shadow"
    };
    return inherited.count(name) > 0;
}

// --- Cascade Value Collection ---

std::vector<CascadeValue> StyleEngine::get_cascade_values(const dom::Node* node, const std::wstring& property,
                                                           bool is_hovered, bool is_focused, bool is_active) {
    std::vector<CascadeValue> values;
    if (!node || node->node_type() != dom::NodeType::Element) return values;

    // UA rules
    for (const auto& rule : m_ua_rules) {
        for (const auto& sel : rule.selectors) {
            if (matches_selector(node, sel, is_hovered, is_focused, is_active)) {
                for (const auto& [prop, val] : rule.declarations) {
                    if (prop == property) {
                        CascadeValue cv;
                        cv.value = val;
                        cv.origin = CascadeOrigin::UA;
                        cv.specificity = calculate_specificity(sel);
                        cv.source_order = rule.source_order;
                        values.push_back(cv);
                    }
                }
            }
        }
    }

    // Author rules
    for (const auto& rule : m_parsed_rules) {
        // Check media query
        if (rule.media_query && !rule.media_query->evaluate(1920, 1080, 1.0f)) continue;
        // Check supports
        if (rule.supports_condition && !rule.supports_condition->evaluate()) continue;

        for (const auto& sel : rule.selectors) {
            if (matches_selector(node, sel, is_hovered, is_focused, is_active)) {
                for (const auto& [prop, val] : rule.declarations) {
                    if (prop == property) {
                        CascadeValue cv;
                        cv.value = val;
                        cv.important = rule.important;
                        cv.origin = CascadeOrigin::Author;
                        cv.specificity = calculate_specificity(sel);
                        cv.source_order = rule.source_order;
                        values.push_back(cv);
                    }
                }
            }
        }
    }

    return values;
}

std::map<std::wstring, std::vector<CascadeValue>> StyleEngine::get_all_cascade_values(
    const dom::Node* node, bool is_hovered, bool is_focused, bool is_active,
    float viewport_width, float viewport_height, float dpr) {
    std::map<std::wstring, std::vector<CascadeValue>> result;
    if (!node || node->node_type() != dom::NodeType::Element) return result;

    // Collect all unique property names referenced in rules
    std::unordered_set<std::wstring> all_props;

    // UA rules
    for (const auto& rule : m_ua_rules) {
        for (const auto& sel : rule.selectors) {
            if (matches_selector(node, sel, is_hovered, is_focused, is_active)) {
                for (const auto& [prop, val] : rule.declarations) {
                    all_props.insert(prop);
                    CascadeValue cv;
                    cv.value = val;
                    cv.origin = CascadeOrigin::UA;
                    cv.specificity = calculate_specificity(sel);
                    cv.source_order = rule.source_order;
                    result[prop].push_back(cv);
                }
            }
        }
    }

    // Author rules
    for (const auto& rule : m_parsed_rules) {
        if (rule.media_query && !rule.media_query->evaluate(viewport_width, viewport_height, dpr)) continue;
        if (rule.supports_condition && !rule.supports_condition->evaluate()) continue;

        for (const auto& sel : rule.selectors) {
            if (matches_selector(node, sel, is_hovered, is_focused, is_active)) {
                for (const auto& [prop, val] : rule.declarations) {
                    all_props.insert(prop);
                    CascadeValue cv;
                    cv.value = val;
                    cv.important = rule.important;
                    cv.origin = CascadeOrigin::Author;
                    cv.specificity = calculate_specificity(sel);
                    cv.source_order = rule.source_order;
                    result[prop].push_back(cv);
                }
            }
        }
    }

    // Sort cascade values for each property
    for (auto& [prop, values] : result) {
        std::stable_sort(values.begin(), values.end(),
            [](const CascadeValue& a, const CascadeValue& b) { return a < b; });
    }

    return result;
}

// --- Property Application ---

void StyleEngine::apply_cascade_value(ComputedStyle& style, const std::wstring& prop, const std::wstring& value) {
    // Handle CSS-wide keywords
    std::wstring trimmed = trim(value);
    if (trimmed == L"inherit") {
        // Handled separately in resolve_inherited_values
        return;
    }
    if (trimmed == L"initial") {
        return; // Initial values set by compute_initial_values
    }
    if (trimmed == L"unset") {
        return; // Depends on property; handled elsewhere
    }
    if (trimmed == L"revert") {
        return; // Handled elsewhere
    }

    parse_property(prop, value, style, false);
}

void StyleEngine::compute_initial_values(ComputedStyle& style) {
    // Already set by ComputedStyle default constructor
}

void StyleEngine::resolve_inherited_values(ComputedStyle& style, const ComputedStyle& parent) {
    // Inherit inherited properties from parent
    if (parent.is_inherited) {
        style.color = parent.color;
        style.font_family = parent.font_family;
        style.font_size = parent.font_size;
        style.font_style = parent.font_style;
        style.font_weight = parent.font_weight;
        style.font_stretch = parent.font_stretch;
        style.line_height = parent.line_height;
        style.letter_spacing = parent.letter_spacing;
        style.word_spacing = parent.word_spacing;
        style.text_align = parent.text_align;
        style.text_transform = parent.text_transform;
        style.text_decoration_color = parent.text_decoration_color;
        style.text_decoration_line = parent.text_decoration_line;
        style.text_decoration_style = parent.text_decoration_style;
        style.text_shadow = parent.text_shadow;
        style.visibility = parent.visibility;
        style.white_space = parent.white_space;
        style.word_break = parent.word_break;
        style.overflow_wrap = parent.overflow_wrap;
        style.direction = parent.direction;
        style.unicode_bidi = parent.unicode_bidi;
        style.cursor = parent.cursor;
        style.is_inherited = true;
    }
}

void StyleEngine::resolve_current_color(ComputedStyle& style) {
    auto [r,g,b,a] = style.color.to_rgba();
    std::wstring color_str = L"rgba(" + std::to_wstring(int(r*255)) + L"," +
                             std::to_wstring(int(g*255)) + L"," +
                             std::to_wstring(int(b*255)) + L"," +
                             std::to_wstring(a) + L")";

    // Resolve border colors set to currentColor
    if (style.border_top_color == L"currentColor") style.border_top_color = color_str;
    if (style.border_right_color == L"currentColor") style.border_right_color = color_str;
    if (style.border_bottom_color == L"currentColor") style.border_bottom_color = color_str;
    if (style.border_left_color == L"currentColor") style.border_left_color = color_str;
    if (style.outline_color == L"currentColor") style.outline_color = color_str;
    if (style.text_decoration_color == L"currentColor") style.text_decoration_color = color_str;
    if (style.caret_color == L"currentColor" || style.caret_color == L"auto") style.caret_color = color_str;
}

// --- Main Compute Style ---

ComputedStyle StyleEngine::compute_style(const dom::Node* node, const ComputedStyle* parent_style,
                                         bool is_hovered, bool is_focused, bool is_active,
                                         float viewport_width, float viewport_height,
                                         float device_pixel_ratio) {
    ComputedStyle style;

    // Step 1: Set initial values
    compute_initial_values(style);

    // Step 2: Inherit from parent
    if (parent_style) {
        style.is_inherited = true;
        resolve_inherited_values(style, *parent_style);
    }

    if (!node || node->node_type() != dom::NodeType::Element) return style;

    const auto* el = static_cast<const dom::Element*>(node);

    // Step 3: Get all cascade values
    auto cascade_map = get_all_cascade_values(node, is_hovered, is_focused, is_active,
                                               viewport_width, viewport_height, device_pixel_ratio);

    // Step 4: Apply cascade (sorted by (important, origin, specificity, source_order))
    for (const auto& [prop, values] : cascade_map) {
        if (!values.empty()) {
            apply_cascade_value(style, prop, values.back().value);
        }
    }

    // Step 5: Apply inline style (highest author priority)
    std::wstring inline_style = el->get_attribute(L"style");
    if (!inline_style.empty()) {
        size_t dp = 0;
        while (dp < inline_style.size()) {
            while (dp < inline_style.size() && (inline_style[dp] == L' ' || inline_style[dp] == L'\t')) dp++;
            if (dp >= inline_style.size()) break;
            size_t colon = inline_style.find(L':', dp);
            if (colon == std::wstring::npos) break;
            std::wstring prop = trim(inline_style.substr(dp, colon - dp));
            size_t semi = inline_style.find(L';', colon + 1);
            if (semi == std::wstring::npos) {
                std::wstring val = trim(inline_style.substr(colon + 1));
                if (!prop.empty()) parse_property(prop, val, style, true);
                break;
            }
            std::wstring val = trim(inline_style.substr(colon + 1, semi - colon - 1));
            if (!prop.empty()) parse_property(prop, val, style, true);
            dp = semi + 1;
        }
    }

    // Step 6: Resolve currentColor
    resolve_current_color(style);

    // Step 7: Set display/inline flags
    style.is_inline = (style.display == L"inline" || style.display == L"inline-block" ||
                       style.display == L"inline-flex" || style.display == L"inline-grid" ||
                       style.display == L"inline-table");
    style.is_replaced = (style.display == L"inline-block" && false); // refined per element type

    return style;
}

void StyleEngine::clear_cache() {
    m_parsed_rules.clear();
    m_ua_rules.clear();
    m_keyframes.clear();
    m_font_faces.clear();
    m_source_order_counter = 0;
}

// --- Property Parser (Phase 3, 12, 13, 14) ---

void StyleEngine::parse_property(const std::wstring& name, const std::wstring& value, ComputedStyle& style, bool is_important) {
    std::wstring val = trim(value);
    if (val.empty()) return;

    // Check for CSS-wide keywords
    if (val == L"inherit") {
        // Will be handled by cascade; skip direct application
        return;
    }
    if (val == L"initial") return;
    if (val == L"unset") return;
    if (val == L"revert") return;

    // Custom properties
    if (name.size() > 2 && name[0] == L'-' && name[1] == L'-') {
        style.custom_properties[name] = CssValue::parse(val);
        return;
    }

    // --- Display & Position ---
    if (name == L"display") {
        style.display = val;
        if (val == L"none") { style.display = L"none"; }
        else if (val == L"block") { style.display = L"block"; }
        else if (val == L"inline") { style.display = L"inline"; }
        else if (val == L"inline-block") { style.display = L"inline-block"; }
        else if (val == L"flex" || val == L"inline-flex") { style.display = val; }
        else if (val == L"grid" || val == L"inline-grid") { style.display = val; }
        else if (val == L"table" || val == L"inline-table") { style.display = val; }
        else if (val == L"table-row" || val == L"table-cell" || val == L"table-caption" ||
                 val == L"table-column" || val == L"table-column-group" ||
                 val == L"table-row-group" || val == L"table-header-group" ||
                 val == L"table-footer-group") { style.display = val; }
        else if (val == L"list-item") { style.display = L"list-item"; }
        else if (val == L"contents") { style.display = L"contents"; }
        return;
    }
    if (name == L"position") { style.position = val; return; }
    if (name == L"float") { style.display = L"block"; return; } // float forces block

    // --- Box Model ---
    if (name == L"width") { style.width = CssValue::parse(val); return; }
    if (name == L"height") { style.height = CssValue::parse(val); return; }
    if (name == L"min-width") { style.min_width = CssValue::parse(val); return; }
    if (name == L"max-width") { style.max_width = CssValue::parse(val); return; }
    if (name == L"min-height") { style.min_height = CssValue::parse(val); return; }
    if (name == L"max-height") { style.max_height = CssValue::parse(val); return; }
    if (name == L"box-sizing") { style.box_sizing = val; return; }

    if (name == L"margin") {
        auto parts = split_whitespace(val);
        if (parts.size() == 1) style.margin_top = style.margin_right = style.margin_bottom = style.margin_left = CssValue::parse(parts[0]);
        else if (parts.size() == 2) { style.margin_top = style.margin_bottom = CssValue::parse(parts[0]); style.margin_right = style.margin_left = CssValue::parse(parts[1]); }
        else if (parts.size() == 3) { style.margin_top = CssValue::parse(parts[0]); style.margin_right = style.margin_left = CssValue::parse(parts[1]); style.margin_bottom = CssValue::parse(parts[2]); }
        else if (parts.size() >= 4) { style.margin_top = CssValue::parse(parts[0]); style.margin_right = CssValue::parse(parts[1]); style.margin_bottom = CssValue::parse(parts[2]); style.margin_left = CssValue::parse(parts[3]); }
        return;
    }
    if (name == L"margin-top") { style.margin_top = CssValue::parse(val); return; }
    if (name == L"margin-right") { style.margin_right = CssValue::parse(val); return; }
    if (name == L"margin-bottom") { style.margin_bottom = CssValue::parse(val); return; }
    if (name == L"margin-left") { style.margin_left = CssValue::parse(val); return; }

    if (name == L"padding") {
        auto parts = split_whitespace(val);
        if (parts.size() == 1) style.padding_top = style.padding_right = style.padding_bottom = style.padding_left = CssValue::parse(parts[0]);
        else if (parts.size() == 2) { style.padding_top = style.padding_bottom = CssValue::parse(parts[0]); style.padding_right = style.padding_left = CssValue::parse(parts[1]); }
        else if (parts.size() == 3) { style.padding_top = CssValue::parse(parts[0]); style.padding_right = style.padding_left = CssValue::parse(parts[1]); style.padding_bottom = CssValue::parse(parts[2]); }
        else if (parts.size() >= 4) { style.padding_top = CssValue::parse(parts[0]); style.padding_right = CssValue::parse(parts[1]); style.padding_bottom = CssValue::parse(parts[2]); style.padding_left = CssValue::parse(parts[3]); }
        return;
    }
    if (name == L"padding-top") { style.padding_top = CssValue::parse(val); return; }
    if (name == L"padding-right") { style.padding_right = CssValue::parse(val); return; }
    if (name == L"padding-bottom") { style.padding_bottom = CssValue::parse(val); return; }
    if (name == L"padding-left") { style.padding_left = CssValue::parse(val); return; }

    if (name == L"border" || name == L"border-width") {
        auto parts = split_whitespace(val);
        for (const auto& p : parts) {
            if (p.find_first_not_of(L"0123456789.-") == std::wstring::npos || p.find(L"px") != std::wstring::npos || p.find(L"em") != std::wstring::npos || p.find(L"%") != std::wstring::npos) {
                style.border_top_width = style.border_right_width = style.border_bottom_width = style.border_left_width = CssValue::parse(p);
            } else if (p.find(L"#") == 0 || p.find(L"rgb") == 0 || p.find(L"hsl") == 0 || p.find(L"lab") == 0 || p.find(L"oklab") == 0 || p.find(L"transparent") == 0 || p.find(L"currentColor") == 0) {
                style.border_top_color = style.border_right_color = style.border_bottom_color = style.border_left_color = p;
            } else if (p == L"solid" || p == L"dotted" || p == L"dashed" || p == L"double" || p == L"groove" || p == L"ridge" || p == L"inset" || p == L"outset" || p == L"none" || p == L"hidden") {
                style.border_top_style = style.border_right_style = style.border_bottom_style = style.border_left_style = p;
            }
        }
        return;
    }
    if (name == L"border-top") { style.border_top_width = CssValue::parse(val); style.border_top = CssValue::parse(val).number; return; }
    if (name == L"border-color") { style.border_top_color = style.border_right_color = style.border_bottom_color = style.border_left_color = val; return; }
    if (name == L"border-top-color") { style.border_top_color = val; return; }
    if (name == L"border-right-color") { style.border_right_color = val; return; }
    if (name == L"border-bottom-color") { style.border_bottom_color = val; return; }
    if (name == L"border-left-color") { style.border_left_color = val; return; }
    if (name == L"border-top-width") { style.border_top_width = CssValue::parse(val); style.border_top = CssValue::parse(val).number; return; }
    if (name == L"border-right-width") { style.border_right_width = CssValue::parse(val); return; }
    if (name == L"border-bottom-width") { style.border_bottom_width = CssValue::parse(val); return; }
    if (name == L"border-left-width") { style.border_left_width = CssValue::parse(val); return; }
    if (name == L"border-style") { style.border_top_style = style.border_right_style = style.border_bottom_style = style.border_left_style = val; return; }
    if (name == L"border-radius") {
        auto parts = split_whitespace(val);
        if (parts.size() == 1) { double r = CssValue::parse(parts[0]).number; style.border_radius_top_left = style.border_radius_top_right = style.border_radius_bottom_right = style.border_radius_bottom_left = r; }
        else if (parts.size() == 2) { double r1 = CssValue::parse(parts[0]).number; double r2 = CssValue::parse(parts[1]).number; style.border_radius_top_left = style.border_radius_bottom_right = r1; style.border_radius_top_right = style.border_radius_bottom_left = r2; }
        else if (parts.size() == 3) { style.border_radius_top_left = CssValue::parse(parts[0]).number; style.border_radius_top_right = style.border_radius_bottom_left = CssValue::parse(parts[1]).number; style.border_radius_bottom_right = CssValue::parse(parts[2]).number; }
        else if (parts.size() >= 4) { style.border_radius_top_left = CssValue::parse(parts[0]).number; style.border_radius_top_right = CssValue::parse(parts[1]).number; style.border_radius_bottom_right = CssValue::parse(parts[2]).number; style.border_radius_bottom_left = CssValue::parse(parts[3]).number; }
        return;
    }
    if (name == L"border-top-left-radius") { style.border_radius_top_left = CssValue::parse(val).number; return; }
    if (name == L"border-top-right-radius") { style.border_radius_top_right = CssValue::parse(val).number; return; }
    if (name == L"border-bottom-right-radius") { style.border_radius_bottom_right = CssValue::parse(val).number; return; }
    if (name == L"border-bottom-left-radius") { style.border_radius_bottom_left = CssValue::parse(val).number; return; }

    // --- Outline ---
    if (name == L"outline") {
        auto parts = split_whitespace(val);
        for (const auto& p : parts) {
            if (p.find(L"#") == 0 || p.find(L"rgb") == 0 || p.find(L"hsl") == 0 || p == L"currentColor" || p == L"transparent" || p == L"invert") style.outline_color = p;
            else if (p == L"solid" || p == L"dotted" || p == L"dashed" || p == L"double" || p == L"groove" || p == L"ridge" || p == L"inset" || p == L"outset" || p == L"none") style.outline_style = p;
            else style.outline_width = CssValue::parse(p);
        }
        return;
    }
    if (name == L"outline-color") { style.outline_color = val; return; }
    if (name == L"outline-style") { style.outline_style = val; return; }
    if (name == L"outline-width") { style.outline_width = CssValue::parse(val); return; }
    if (name == L"outline-offset") { style.outline_offset = CssValue::parse(val); return; }

    // --- Position ---
    if (name == L"top") { style.top = CssValue::parse(val); return; }
    if (name == L"right") { style.right = CssValue::parse(val); return; }
    if (name == L"bottom") { style.bottom = CssValue::parse(val); return; }
    if (name == L"left") { style.left = CssValue::parse(val); return; }
    if (name == L"inset") {
        auto parts = split_whitespace(val);
        if (parts.size() == 1) style.top = style.right = style.bottom = style.left = CssValue::parse(parts[0]);
        else if (parts.size() == 2) { style.top = style.bottom = CssValue::parse(parts[0]); style.right = style.left = CssValue::parse(parts[1]); }
        else if (parts.size() == 4) { style.top = CssValue::parse(parts[0]); style.right = CssValue::parse(parts[1]); style.bottom = CssValue::parse(parts[2]); style.left = CssValue::parse(parts[3]); }
        return;
    }
    if (name == L"z-index") { style.z_index = (int)CssValue::parse(val).number; style.has_z_index = true; return; }

    // --- Overflow ---
    if (name == L"overflow") { style.overflow_x = style.overflow_y = val; style.overflow = val; return; }
    if (name == L"overflow-x") { style.overflow_x = val; return; }
    if (name == L"overflow-y") { style.overflow_y = val; return; }

    // --- Typography (Phase 12) ---
    if (name == L"font-family") { style.font_family = val; return; }
    if (name == L"font-size") { style.font_size = CssValue::parse(val); return; }
    if (name == L"font-weight") {
        if (val == L"normal") style.font_weight = L"normal";
        else if (val == L"bold") style.font_weight = L"bold";
        else if (val == L"bolder") { /* increase weight */ }
        else if (val == L"lighter") { /* decrease weight */ }
        else { try { int w = std::stoi(val); if (w >= 100 && w <= 900) style.font_weight = val; } catch(...) {} }
        return;
    }
    if (name == L"font-style") { style.font_style = val; return; }
    if (name == L"font-stretch") { style.font_stretch = val; return; }
    if (name == L"font-variant") { style.font_variant = val; return; }
    if (name == L"font") {
        // Shorthand: font-style font-variant font-weight font-stretch font-size/line-height font-family
        // Simplified: just extract font-size and font-family
        auto parts = split_whitespace(val);
        for (size_t i = 0; i < parts.size(); i++) {
            if (parts[i] == L"italic" || parts[i] == L"oblique") style.font_style = parts[i];
            else if (parts[i] == L"bold" || parts[i] == L"bolder" || parts[i] == L"lighter" ||
                     (parts[i].find_first_not_of(L"0123456789") == std::wstring::npos && std::stoi(parts[i]) % 100 == 0)) style.font_weight = parts[i];
            else if (parts[i].find(L'/') != std::wstring::npos) {
                size_t slash = parts[i].find(L'/');
                style.font_size = CssValue::parse(parts[i].substr(0, slash));
                style.line_height = CssValue::parse(parts[i].substr(slash + 1));
            } else if (parts[i].find(L"px") != std::wstring::npos || parts[i].find(L"em") != std::wstring::npos ||
                       parts[i].find(L"pt") != std::wstring::npos || parts[i].find(L"%") != std::wstring::npos) {
                style.font_size = CssValue::parse(parts[i]);
            } else if (parts[i][0] == L'-' || parts[i][0] == L'\"' || parts[i][0] == L'\'') {
                style.font_family = parts[i];
            }
        }
        return;
    }

    if (name == L"line-height") { style.line_height = CssValue::parse(val); return; }
    if (name == L"letter-spacing") { style.letter_spacing = CssValue::parse(val); return; }
    if (name == L"word-spacing") { style.word_spacing = CssValue::parse(val); return; }
    if (name == L"text-align") { style.text_align = val; return; }
    if (name == L"text-indent") { /* store */ return; }
    if (name == L"text-transform") { style.text_transform = val; return; }

    if (name == L"text-decoration") {
        auto parts = split_whitespace(val);
        for (const auto& p : parts) {
            if (p == L"underline" || p == L"overline" || p == L"line-through" || p == L"blink" || p == L"none") style.text_decoration_line = p;
            else if (p == L"solid" || p == L"double" || p == L"dotted" || p == L"dashed" || p == L"wavy") style.text_decoration_style = p;
            else if (p.find(L"#") == 0 || p.find(L"rgb") == 0 || p.find(L"hsl") == 0 || p == L"currentColor" || p == L"transparent") style.text_decoration_color = p;
            else style.text_decoration_thickness = CssValue::parse(p);
        }
        return;
    }
    if (name == L"text-decoration-line") { style.text_decoration_line = val; return; }
    if (name == L"text-decoration-color") { style.text_decoration_color = val; return; }
    if (name == L"text-decoration-style") { style.text_decoration_style = val; return; }
    if (name == L"text-decoration-thickness") { style.text_decoration_thickness = CssValue::parse(val); return; }
    if (name == L"text-shadow") {
        if (val == L"none") { style.text_shadow = L"none"; return; }
        style.text_shadow = val;
        return;
    }
    if (name == L"direction") { style.direction = val; return; }
    if (name == L"unicode-bidi") { style.unicode_bidi = val; return; }
    if (name == L"white-space") { style.white_space = val; return; }
    if (name == L"word-break") { style.word_break = val; return; }
    if (name == L"overflow-wrap") { style.overflow_wrap = val; return; }

    // --- Colors (Phase 13) ---
    if (name == L"color") { style.color = CssColor::parse(val); return; }
    if (name == L"background-color") { style.background_color = CssColor::parse(val); return; }

    if (name == L"background") {
        // Parse background shorthand - simplified
        if (val.find(L"linear-gradient") == 0 || val.find(L"radial-gradient") == 0 ||
            val.find(L"conic-gradient") == 0 || val.find(L"repeating-linear-gradient") == 0 ||
            val.find(L"repeating-radial-gradient") == 0 || val.find(L"repeating-conic-gradient") == 0) {
            style.background_gradient = val;
            style.background_image = val;
        } else if (val.find(L"url(") == 0) {
            style.background_image = val;
        } else if (val == L"none") {
            style.background_image = L"none";
            style.background_gradient = L"";
            style.background_color = CssColor();
        } else {
            // Try to parse as color
            CssColor c = CssColor::parse(val);
            if (!c.is_transparent || val == L"transparent") style.background_color = c;
        }
        return;
    }

    if (name == L"background-image") {
        style.background_image = val;
        if (val.find(L"gradient") != std::wstring::npos) style.background_gradient = val;
        return;
    }
    if (name == L"background-repeat") { style.background_repeat = val; return; }
    if (name == L"background-position") { style.background_position = val; return; }

    if (name == L"background-size") {
        style.background_size = val;
        return;
    }
    if (name == L"background-attachment") { style.background_attachment = val; return; }
    if (name == L"background-clip") { style.background_clip = val; return; }
    if (name == L"background-origin") { style.background_origin = val; return; }

    // Opacity
    if (name == L"opacity") {
        style.opacity = CssValue::parse(val);
        if (style.opacity.unit == CssUnit::None && style.opacity.number == 0 && val != L"0") {
            style.opacity.number = std::stod(val);
        }
        return;
    }

    // Visibility
    if (name == L"visibility") { style.visibility = val; return; }
    if (name == L"cursor") { style.cursor = val; return; }
    if (name == L"pointer-events") { style.pointer_events = val; return; }

    // Box Shadow
    if (name == L"box-shadow") {
        if (val == L"none") { style.box_shadow_string = L"none"; style.box_shadows.clear(); return; }
        style.box_shadow_string = val;
        // Parse shadow values: offset-x offset-y blur-radius spread-radius color
        auto parts = split_whitespace(val);
        if (parts.size() >= 2) {
            double ox = CssValue::parse(parts[0]).number;
            double oy = CssValue::parse(parts[1]).number;
            double blur = parts.size() >= 3 ? CssValue::parse(parts[2]).number : 0;
            std::wstring color = parts.size() >= 4 ? parts[3] : L"currentColor";
            if (parts.size() >= 5) color = parts[4];
            style.box_shadows.push_back({ox, oy, blur, CssColor::parse(color)});
        }
        return;
    }

    // --- Flexbox ---
    if (name == L"flex-direction") { style.flex_direction = val; return; }
    if (name == L"flex-wrap") { style.flex_wrap = val; return; }
    if (name == L"flex-flow") {
        auto parts = split_whitespace(val);
        for (const auto& p : parts) {
            if (p == L"row" || p == L"row-reverse" || p == L"column" || p == L"column-reverse") style.flex_direction = p;
            if (p == L"nowrap" || p == L"wrap" || p == L"wrap-reverse") style.flex_wrap = p;
        }
        return;
    }
    if (name == L"justify-content") { style.justify_content = val; return; }
    if (name == L"align-items") { style.align_items = val; return; }
    if (name == L"align-content") { style.align_content = val; return; }
    if (name == L"align-self") { style.align_self = val; return; }
    if (name == L"flex") {
        auto parts = split_whitespace(val);
        if (parts.size() >= 1) style.flex_grow = CssValue::parse(parts[0]).number;
        if (parts.size() >= 2) style.flex_shrink = CssValue::parse(parts[1]).number;
        if (parts.size() >= 3) style.flex_basis = CssValue::parse(parts[2]);
        else if (parts.size() == 1 && parts[0] != L"0" && parts[0] != L"1") style.flex_basis = CssValue::parse(parts[0]);
        return;
    }
    if (name == L"flex-grow") { style.flex_grow = CssValue::parse(val).number; return; }
    if (name == L"flex-shrink") { style.flex_shrink = CssValue::parse(val).number; return; }
    if (name == L"flex-basis") { style.flex_basis = CssValue::parse(val); return; }
    if (name == L"order") { style.order = (int)CssValue::parse(val).number; return; }

    // --- Grid (Phase 9) ---
    if (name == L"grid-template-columns") { style.grid_template_columns = val; return; }
    if (name == L"grid-template-rows") { style.grid_template_rows = val; return; }
    if (name == L"grid-template-areas") { style.grid_template_areas = val; return; }
    if (name == L"grid-template") {
        // Simplified: not handling complex template shorthand
        return;
    }
    if (name == L"grid-auto-columns") { style.grid_auto_columns = val; return; }
    if (name == L"grid-auto-rows") { style.grid_auto_rows = val; return; }
    if (name == L"grid-auto-flow") { style.grid_auto_flow = val; return; }
    if (name == L"grid-column-start") { style.grid_column_start = val; return; }
    if (name == L"grid-column-end") { style.grid_column_end = val; return; }
    if (name == L"grid-row-start") { style.grid_row_start = val; return; }
    if (name == L"grid-row-end") { style.grid_row_end = val; return; }
    if (name == L"grid-column") { style.grid_column = val; return; }
    if (name == L"grid-row") { style.grid_row = val; return; }
    if (name == L"grid-area") { style.grid_area = val; return; }
    if (name == L"grid") {
        // Simplified shorthand
        return;
    }
    if (name == L"gap" || name == L"grid-gap") {
        auto parts = split_whitespace(val);
        if (parts.size() >= 1) style.row_gap = style.column_gap = parts[0];
        if (parts.size() >= 2) style.column_gap = parts[1];
        style.gap = val;
        return;
    }
    if (name == L"row-gap" || name == L"grid-row-gap") { style.row_gap = val; style.gap = val; return; }
    if (name == L"column-gap" || name == L"grid-column-gap") { style.column_gap = val; style.gap = val; return; }

    // --- Transforms (Phase 14) ---
    if (name == L"transform") { style.transform = val; return; }
    if (name == L"transform-origin") { style.transform_origin = val; return; }
    if (name == L"transform-style") { style.transform_style = val; return; }
    if (name == L"perspective") { style.perspective = val; return; }
    if (name == L"perspective-origin") { style.perspective_origin = val; return; }
    if (name == L"backface-visibility") { style.backface_visibility = val; return; }

    // --- Filters (Phase 15) ---
    if (name == L"filter") { style.filter = val; return; }
    if (name == L"backdrop-filter") { style.backdrop_filter = val; return; }

    // --- Transitions (Phase 18) ---
    if (name == L"transition") {
        if (val == L"none") { style.transition_property = L"none"; style.transitions.clear(); return; }
        // Parse: property duration delay timing-function
        auto parts = split_whitespace(val);
        style.transition_property = parts.size() >= 1 ? parts[0] : L"all";
        style.transition_duration = parts.size() >= 2 ? CssValue::parse(parts[1]).to_seconds() : 0;
        style.transition_delay = parts.size() >= 3 ? CssValue::parse(parts[2]).to_seconds() : 0;
        style.transition_timing_function = parts.size() >= 4 ? parts[3] : L"ease";
        style.transitions.push_back({style.transition_property, style.transition_duration, style.transition_delay, style.transition_timing_function});
        return;
    }
    if (name == L"transition-property") { style.transition_property = val; return; }
    if (name == L"transition-duration") { style.transition_duration = CssValue::parse(val).to_seconds(); return; }
    if (name == L"transition-delay") { style.transition_delay = CssValue::parse(val).to_seconds(); return; }
    if (name == L"transition-timing-function") { style.transition_timing_function = val; return; }

    // --- Animations (Phase 17) ---
    if (name == L"animation") {
        if (val == L"none") { style.animation_name = L"none"; style.animations.clear(); return; }
        auto parts = split_whitespace(val);
        if (parts.size() >= 1) {
            style.animation_name = parts[0];
            style.animation_duration = parts.size() >= 2 ? CssValue::parse(parts[1]).to_seconds() : 0;
            style.animation_timing_function = parts.size() >= 3 ? parts[2] : L"ease";
            style.animation_delay = parts.size() >= 4 ? CssValue::parse(parts[3]).to_seconds() : 0;
            if (parts.size() >= 5) {
                if (parts[4] == L"infinite") style.animation_iteration_count = -1;
                else style.animation_iteration_count = (int)CssValue::parse(parts[4]).number;
            }
            if (parts.size() >= 6) {
                if (parts[5] == L"normal" || parts[5] == L"reverse" || parts[5] == L"alternate" || parts[5] == L"alternate-reverse")
                    style.animation_direction = parts[5];
            }
            if (parts.size() >= 7) {
                if (parts[6] == L"none" || parts[6] == L"forwards" || parts[6] == L"backwards" || parts[6] == L"both")
                    style.animation_fill_mode = parts[6];
            }
            if (parts.size() >= 8) style.animation_play_state = parts[7];
            style.animations.push_back({style.animation_name, style.animation_duration, style.animation_delay,
                                        style.animation_iteration_count, style.animation_direction,
                                        style.animation_fill_mode, style.animation_play_state,
                                        style.animation_timing_function, L""});
        }
        return;
    }
    if (name == L"animation-name") { style.animation_name = val; return; }
    if (name == L"animation-duration") { style.animation_duration = CssValue::parse(val).to_seconds(); return; }
    if (name == L"animation-delay") { style.animation_delay = CssValue::parse(val).to_seconds(); return; }
    if (name == L"animation-iteration-count") { style.animation_iteration_count = (int)CssValue::parse(val).number; return; }
    if (name == L"animation-direction") { style.animation_direction = val; return; }
    if (name == L"animation-fill-mode") { style.animation_fill_mode = val; return; }
    if (name == L"animation-play-state") { style.animation_play_state = val; return; }
    if (name == L"animation-timing-function") { style.animation_timing_function = val; return; }

    // --- Containment ---
    if (name == L"contain") { style.contain = val; return; }
    if (name == L"content-visibility") { style.content_visibility = val; return; }
    if (name == L"container-type") { style.container_type = val; return; }
    if (name == L"container-name") { style.container_name = val; return; }
    if (name == L"container") {
        auto parts = split_whitespace(val);
        if (parts.size() >= 1 && parts[0] != L"none") style.container_name = parts[0];
        if (parts.size() >= 2) style.container_type = parts[1];
        return;
    }

    // --- Appearance ---
    if (name == L"appearance" || name == L"-webkit-appearance") { style.appearance = val; return; }
    if (name == L"accent-color") { style.accent_color = val; return; }
    if (name == L"caret-color") { style.caret_color = val; return; }

    // --- Mix-blend-mode, isolation ---
    if (name == L"mix-blend-mode") { /* store */ return; }
    if (name == L"isolation") { /* store */ return; }
    if (name == L"clip-path") { /* store */ return; }
    if (name == L"mask") { /* store */ return; }
    if (name == L"mask-image") { /* store */ return; }
    if (name == L"will-change") { /* store */ return; }

    // --- List style ---
    if (name == L"list-style") { /* store */ return; }
    if (name == L"list-style-type") { /* store */ return; }
    if (name == L"list-style-image") { /* store */ return; }
    if (name == L"list-style-position") { /* store */ return; }

    // --- Table ---
    if (name == L"table-layout") { /* store */ return; }
    if (name == L"border-collapse") { /* store */ return; }
    if (name == L"border-spacing") { /* store */ return; }
    if (name == L"empty-cells") { /* store */ return; }
    if (name == L"caption-side") { /* store */ return; }

    // --- Counter ---
    if (name == L"counter-increment") { /* store */ return; }
    if (name == L"counter-reset") { /* store */ return; }
    if (name == L"counter-set") { /* store */ return; }
    if (name == L"content") { /* store */ return; }
    if (name == L"quotes") { /* store */ return; }

    // --- Columns ---
    if (name == L"columns") { /* store */ return; }
    if (name == L"column-count") { /* store */ return; }
    if (name == L"column-width") { /* store */ return; }
    if (name == L"column-gap") { style.column_gap = val; return; }
    if (name == L"column-rule") { /* store */ return; }
    if (name == L"column-rule-color") { /* store */ return; }
    if (name == L"column-rule-style") { /* store */ return; }
    if (name == L"column-rule-width") { /* store */ return; }
    if (name == L"column-span") { /* store */ return; }
    if (name == L"column-fill") { /* store */ return; }

    // --- Page ---
    if (name == L"page-break-before") { /* store */ return; }
    if (name == L"page-break-after") { /* store */ return; }
    if (name == L"page-break-inside") { /* store */ return; }
    if (name == L"orphans") { /* store */ return; }
    if (name == L"widows") { /* store */ return; }

    // --- Image ---
    if (name == L"object-fit") { /* store */ return; }
    if (name == L"object-position") { /* store */ return; }
    if (name == L"image-rendering") { /* store */ return; }

    // --- Scroll ---
    if (name == L"scroll-behavior") { /* store */ return; }
    if (name == L"overscroll-behavior") { /* store */ return; }
    if (name == L"scroll-snap-type") { /* store */ return; }
    if (name == L"scroll-snap-align") { /* store */ return; }
    if (name == L"scroll-margin") { /* store */ return; }
    if (name == L"scroll-padding") { /* store */ return; }

    // --- SVG ---
    if (name == L"fill") { /* store */ return; }
    if (name == L"stroke") { /* store */ return; }
    if (name == L"stroke-width") { /* store */ return; }

    // --- User select ---
    if (name == L"user-select") { /* store */ return; }
    if (name == L"-webkit-user-select") { /* store */ return; }

    // --- Print ---
    if (name == L"print-color-adjust") { /* store */ return; }
    if (name == L"-webkit-print-color-adjust") { /* store */ return; }
}

// --- Specificity (Phase 2: Task 8-9) ---

Specificity StyleEngine::calculate_specificity(const std::wstring& selector) {
    return SelectorExtensions::specificity_of(selector);
}

// --- Selector Matching (Phase 2) ---

bool StyleEngine::matches_selector(const dom::Node* node, const std::wstring& selector,
                                    bool is_hovered, bool is_focused, bool is_active) {
    if (!node) return false;
    if (node->node_type() != dom::NodeType::Element) return false;

    const auto* el = static_cast<const dom::Element*>(node);
    if (selector.empty()) return false;

    std::wstring sel = selector;

    // Handle pseudo-classes and pseudo-elements in the selector
    size_t pseudo_pos = 0;
    while (pseudo_pos < sel.size()) {
        if (sel[pseudo_pos] == L':') {
            if (pseudo_pos + 1 < sel.size() && sel[pseudo_pos + 1] == L':') {
                // ::pseudo-element - strip it
                size_t end = pseudo_pos + 2;
                while (end < sel.size() && (std::iswalnum(sel[end]) || sel[end] == L'-')) end++;
                sel.erase(pseudo_pos, end - pseudo_pos);
                continue;
            }
            // :pseudo-class
            if (pseudo_pos + 6 <= sel.size() && sel.substr(pseudo_pos, 6) == L":hover") {
                if (!is_hovered) return false;
                sel.erase(pseudo_pos, 6); continue;
            }
            if (pseudo_pos + 6 <= sel.size() && sel.substr(pseudo_pos, 6) == L":focus") {
                if (!is_focused) return false;
                sel.erase(pseudo_pos, 6); continue;
            }
            if (pseudo_pos + 7 <= sel.size() && sel.substr(pseudo_pos, 7) == L":active") {
                if (!is_active) return false;
                sel.erase(pseudo_pos, 7); continue;
            }
            if (pseudo_pos + 10 <= sel.size() && sel.substr(pseudo_pos, 10) == L":nth-child") {
                size_t paren = sel.find(L'(', pseudo_pos);
                if (paren != std::wstring::npos) {
                    std::wstring expr = extract_paren_content(sel, paren);
                    auto match = SelectorExtensions::matches_nth_child(node, expr);
                    if (!match.matches) return false;
                    size_t end = sel.find(L')', paren) + 1;
                    sel.erase(pseudo_pos, end - pseudo_pos); continue;
                }
            }
            if (pseudo_pos + 13 <= sel.size() && sel.substr(pseudo_pos, 13) == L":nth-of-type") {
                size_t paren = sel.find(L'(', pseudo_pos);
                if (paren != std::wstring::npos) {
                    std::wstring expr = extract_paren_content(sel, paren);
                    auto match = SelectorExtensions::matches_nth_of_type(node, expr);
                    if (!match.matches) return false;
                    size_t end = sel.find(L')', paren) + 1;
                    sel.erase(pseudo_pos, end - pseudo_pos); continue;
                }
            }
            if (pseudo_pos + 4 <= sel.size() && sel.substr(pseudo_pos, 4) == L":not") {
                size_t paren = sel.find(L'(', pseudo_pos);
                if (paren != std::wstring::npos) {
                    std::wstring args = extract_paren_content(sel, paren);
                    auto list = split_comma_list(args);
                    auto match = SelectorExtensions::matches_not(node, list);
                    if (!match.matches) return false;
                    size_t end = sel.find(L')', paren) + 1;
                    sel.erase(pseudo_pos, end - pseudo_pos); continue;
                }
            }
            if (pseudo_pos + 3 <= sel.size() && sel.substr(pseudo_pos, 3) == L":is") {
                size_t paren = sel.find(L'(', pseudo_pos);
                if (paren != std::wstring::npos) {
                    std::wstring args = extract_paren_content(sel, paren);
                    auto list = split_comma_list(args);
                    auto match = SelectorExtensions::matches_is(node, list);
                    if (!match.matches) return false;
                    size_t end = sel.find(L')', paren) + 1;
                    sel.erase(pseudo_pos, end - pseudo_pos); continue;
                }
            }
            if (pseudo_pos + 6 <= sel.size() && sel.substr(pseudo_pos, 6) == L":where") {
                size_t paren = sel.find(L'(', pseudo_pos);
                if (paren != std::wstring::npos) {
                    std::wstring args = extract_paren_content(sel, paren);
                    auto list = split_comma_list(args);
                    auto match = SelectorExtensions::matches_where(node, list);
                    if (!match.matches) return false;
                    size_t end = sel.find(L')', paren) + 1;
                    sel.erase(pseudo_pos, end - pseudo_pos); continue;
                }
            }
            if (pseudo_pos + 4 <= sel.size() && sel.substr(pseudo_pos, 4) == L":has") {
                size_t paren = sel.find(L'(', pseudo_pos);
                if (paren != std::wstring::npos) {
                    std::wstring args = extract_paren_content(sel, paren);
                    auto match = SelectorExtensions::matches_has(node, args);
                    if (!match.matches) return false;
                    size_t end = sel.find(L')', paren) + 1;
                    sel.erase(pseudo_pos, end - pseudo_pos); continue;
                }
            }
            if (pseudo_pos + 5 <= sel.size() && sel.substr(pseudo_pos, 5) == L":lang") {
                size_t paren = sel.find(L'(', pseudo_pos);
                if (paren != std::wstring::npos) {
                    std::wstring args = extract_paren_content(sel, paren);
                    auto match = SelectorExtensions::matches_lang(node, trim(args));
                    if (!match.matches) return false;
                    size_t end = sel.find(L')', paren) + 1;
                    sel.erase(pseudo_pos, end - pseudo_pos); continue;
                }
            }
            if (pseudo_pos + 14 <= sel.size() && sel.substr(pseudo_pos, 14) == L":first-child") {
                auto match = SelectorExtensions::matches_nth_child(node, L"1");
                if (!match.matches) return false;
                sel.erase(pseudo_pos, 14); continue;
            }
            if (pseudo_pos + 13 <= sel.size() && sel.substr(pseudo_pos, 13) == L":last-child") {
                // Check if this is the last child
                auto parent_shared = node->parent_node();
                if (!parent_shared) return false;
                const auto& children = parent_shared->child_nodes();
                bool is_last = false;
                for (auto it = children.rbegin(); it != children.rend(); ++it) {
                    if (dynamic_cast<const dom::Element*>(it->get())) {
                        is_last = (it->get() == node);
                        break;
                    }
                }
                if (!is_last) return false;
                sel.erase(pseudo_pos, 13); continue;
            }
            if (pseudo_pos + 14 <= sel.size() && sel.substr(pseudo_pos, 14) == L":first-of-type") {
                auto match = SelectorExtensions::matches_nth_of_type(node, L"1");
                if (!match.matches) return false;
                sel.erase(pseudo_pos, 14); continue;
            }
            if (pseudo_pos + 10 <= sel.size() && sel.substr(pseudo_pos, 10) == L":empty") {
                if (!node->child_nodes().empty()) return false;
                sel.erase(pseudo_pos, 10); continue;
            }
            if (pseudo_pos + 12 <= sel.size() && sel.substr(pseudo_pos, 12) == L":only-child") {
                int count = 0;
                auto parent_shared = node->parent_node();
                if (parent_shared) {
                    for (const auto& child : parent_shared->child_nodes()) {
                        if (dynamic_cast<const dom::Element*>(child.get())) count++;
                    }
                }
                if (count != 1) return false;
                sel.erase(pseudo_pos, 12); continue;
            }
            if (pseudo_pos + 13 <= sel.size() && sel.substr(pseudo_pos, 13) == L":only-of-type") {
                auto parent_shared = node->parent_node();
                if (!parent_shared) return false;
                const auto* elem = dynamic_cast<const dom::Element*>(node);
                if (!elem) return false;
                int count = 0;
                for (const auto& child : parent_shared->child_nodes()) {
                    const auto* ce = dynamic_cast<const dom::Element*>(child.get());
                    if (ce && ce->tag_name() == elem->tag_name()) count++;
                }
                if (count != 1) return false;
                sel.erase(pseudo_pos, 13); continue;
            }
            // Unknown pseudo-class - skip it
            pseudo_pos++;
        } else {
            pseudo_pos++;
        }
    }

    sel = trim(sel);
    if (sel.empty() || sel == L"*") return true;

    // Handle attribute selectors
    size_t bracket = sel.find(L'[');
    if (bracket != std::wstring::npos) {
        std::wstring before_attr = trim(sel.substr(0, bracket));
        if (!before_attr.empty() && before_attr != L"*") {
            if (before_attr[0] == L'#') { if (el->id() != before_attr.substr(1)) return false; }
            else if (before_attr[0] == L'.') { if (!std::count(el->class_list().begin(), el->class_list().end(), before_attr.substr(1))) return false; }
            else if (!SelectorExtensions::matches_simple_selector(node, before_attr)) return false;
        }
        size_t pos = 0;
        while ((pos = sel.find(L'[', pos)) != std::wstring::npos) {
            size_t end = sel.find(L']', pos);
            if (end == std::wstring::npos) break;
            std::wstring attr_sel = sel.substr(pos, end - pos + 1);
            if (!SelectorExtensions::matches_attribute(node, attr_sel)) return false;
            pos = end + 1;
        }
        return true;
    }

    if (sel[0] == L'#') return el->id() == sel.substr(1);
    if (sel[0] == L'.') {
        std::wstring cls = sel.substr(1);
        return std::find(el->class_list().begin(), el->class_list().end(), cls) != el->class_list().end();
    }

    size_t dot = sel.find(L'.');
    if (dot != std::wstring::npos) {
        std::wstring tag = sel.substr(0, dot);
        std::wstring cls = sel.substr(dot + 1);
        if (!tag.empty() && tag != el->tag_name()) return false;
        return std::find(el->class_list().begin(), el->class_list().end(), cls) != el->class_list().end();
    }

    return el->tag_name() == sel;
}

} // namespace hre::css
