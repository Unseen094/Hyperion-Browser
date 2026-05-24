#include <hre/css/css_filters.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace hre::css {

filter_engine::filter_engine() = default;
filter_engine::~filter_engine() = default;

css_filter_list filter_engine::parse_filters(const std::wstring& filter_string) {
    css_filter_list list;
    if (filter_string.empty() || filter_string == L"none") return list;

    size_t pos = 0;
    while (pos < filter_string.size()) {
        while (pos < filter_string.size() && (filter_string[pos] == L' ' || filter_string[pos] == L',')) ++pos;
        if (pos >= filter_string.size()) break;

        size_t paren = filter_string.find(L'(', pos);
        if (paren == std::wstring::npos) break;

        std::wstring func_name = filter_string.substr(pos, paren - pos);
        size_t close = filter_string.find(L')', paren);
        if (close == std::wstring::npos) break;

        std::wstring args = filter_string.substr(paren + 1, close - paren - 1);
        pos = close + 1;

        css_filter_function func;

        if (func_name == L"blur") {
            func.type = filter_function_type::BLUR;
            func.amount = std::stod(args);
        } else if (func_name == L"brightness") {
            func.type = filter_function_type::BRIGHTNESS;
            func.amount = std::stod(args);
        } else if (func_name == L"contrast") {
            func.type = filter_function_type::CONTRAST;
            func.amount = std::stod(args);
        } else if (func_name == L"drop-shadow") {
            func.type = filter_function_type::DROP_SHADOW;
            size_t sp1 = args.find(L' ');
            if (sp1 != std::wstring::npos) {
                func.shadow_offset_x = std::stod(args.substr(0, sp1));
                size_t sp2 = args.find(L' ', sp1 + 1);
                if (sp2 != std::wstring::npos) {
                    func.shadow_offset_y = std::stod(args.substr(sp1 + 1, sp2 - sp1 - 1));
                    func.shadow_blur = std::stod(args.substr(sp2 + 1));
                } else {
                    func.shadow_offset_y = std::stod(args.substr(sp1 + 1));
                }
            }
        } else if (func_name == L"grayscale") {
            func.type = filter_function_type::GRAYSCALE;
            func.amount = args.empty() ? 1.0 : std::stod(args);
        } else if (func_name == L"hue-rotate") {
            func.type = filter_function_type::HUE_ROTATE;
            func.amount2 = std::stod(args);
        } else if (func_name == L"invert") {
            func.type = filter_function_type::INVERT;
            func.amount = args.empty() ? 1.0 : std::stod(args);
        } else if (func_name == L"opacity") {
            func.type = filter_function_type::OPACITY;
            func.amount = args.empty() ? 1.0 : std::stod(args);
        } else if (func_name == L"saturate") {
            func.type = filter_function_type::SATURATE;
            func.amount = std::stod(args);
        } else if (func_name == L"sepia") {
            func.type = filter_function_type::SEPIA;
            func.amount = args.empty() ? 1.0 : std::stod(args);
        } else if (func_name.find(L"url(") == 0) {
            func.type = filter_function_type::URL;
            func.url = args;
        }

        list.add(func);
    }

    return list;
}

css_filter_list filter_engine::parse_backdrop_filter(const std::wstring& filter_string) {
    return parse_filters(filter_string);
}

bool filter_engine::apply_filters(const css_filter_list& filters,
                                   uint8_t* pixels, int width, int height, int stride) {
    for (const auto& f : filters.filters) {
        switch (f.type) {
            case filter_function_type::BLUR:
                fast_blur(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::BRIGHTNESS:
                brightness(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::CONTRAST:
                contrast(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::GRAYSCALE:
                grayscale(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::SEPIA:
                sepia(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::INVERT:
                invert(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::SATURATE:
                saturate(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::HUE_ROTATE:
                hue_rotate(pixels, width, height, stride, f.amount2);
                break;
            case filter_function_type::OPACITY:
                opacity(pixels, width, height, stride, f.amount);
                break;
            case filter_function_type::DROP_SHADOW:
                drop_shadow(pixels, width, height, stride, f.shadow_offset_x, f.shadow_offset_y, f.shadow_blur, f.shadow_color);
                break;
            default:
                break;
        }
    }
    return true;
}

bool filter_engine::gaussian_blur(uint8_t* pixels, int width, int height, int stride, double radius) {
    return fast_blur(pixels, width, height, stride, radius);
}

bool filter_engine::box_blur(uint8_t* pixels, int width, int height, int stride, int radius) {
    std::vector<uint8_t> temp(width * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < 4; ++c) {
                int sum = 0, count = 0;
                for (int kx = -radius; kx <= radius; ++kx) {
                    int px = std::clamp(x + kx, 0, width - 1);
                    sum += pixels[y * stride + px * 4 + c];
                    ++count;
                }
                temp[y * width * 4 + x * 4 + c] = static_cast<uint8_t>(sum / count);
            }
        }
    }
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            for (int c = 0; c < 4; ++c) {
                int sum = 0, count = 0;
                for (int ky = -radius; ky <= radius; ++ky) {
                    int py = std::clamp(y + ky, 0, height - 1);
                    sum += temp[py * width * 4 + x * 4 + c];
                    ++count;
                }
                pixels[y * stride + x * 4 + c] = static_cast<uint8_t>(sum / count);
            }
        }
    }
    return true;
}

bool filter_engine::fast_blur(uint8_t* pixels, int width, int height, int stride, double radius) {
    int r = static_cast<int>(std::ceil(radius));
    if (r <= 0) return true;
    return box_blur(pixels, width, height, stride, r);
}

bool filter_engine::apply_color_matrix(uint8_t* pixels, int width, int height, int stride,
                                        const double matrix[20]) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            double r = pixels[idx] / 255.0;
            double g = pixels[idx + 1] / 255.0;
            double b = pixels[idx + 2] / 255.0;
            double a = pixels[idx + 3] / 255.0;

            double nr = matrix[0] * r + matrix[1] * g + matrix[2] * b + matrix[3] * a + matrix[4];
            double ng = matrix[5] * r + matrix[6] * g + matrix[7] * b + matrix[8] * a + matrix[9];
            double nb = matrix[10] * r + matrix[11] * g + matrix[12] * b + matrix[13] * a + matrix[14];
            double na = matrix[15] * r + matrix[16] * g + matrix[17] * b + matrix[18] * a + matrix[19];

            pixels[idx] = static_cast<uint8_t>(std::clamp(nr * 255.0, 0.0, 255.0));
            pixels[idx + 1] = static_cast<uint8_t>(std::clamp(ng * 255.0, 0.0, 255.0));
            pixels[idx + 2] = static_cast<uint8_t>(std::clamp(nb * 255.0, 0.0, 255.0));
            pixels[idx + 3] = static_cast<uint8_t>(std::clamp(na * 255.0, 0.0, 255.0));
        }
    }
    return true;
}

bool filter_engine::brightness(uint8_t* pixels, int width, int height, int stride, double amount) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            pixels[idx] = static_cast<uint8_t>(std::clamp(pixels[idx] * amount, 0.0, 255.0));
            pixels[idx + 1] = static_cast<uint8_t>(std::clamp(pixels[idx + 1] * amount, 0.0, 255.0));
            pixels[idx + 2] = static_cast<uint8_t>(std::clamp(pixels[idx + 2] * amount, 0.0, 255.0));
        }
    }
    return true;
}

bool filter_engine::contrast(uint8_t* pixels, int width, int height, int stride, double amount) {
    double factor = (259.0 * (amount * 128.0 + 255.0)) / (255.0 * (259.0 - amount * 128.0));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            for (int c = 0; c < 3; ++c) {
                double v = pixels[idx + c] / 255.0;
                v = (v - 0.5) * factor + 0.5;
                pixels[idx + c] = static_cast<uint8_t>(std::clamp(v * 255.0, 0.0, 255.0));
            }
        }
    }
    return true;
}

bool filter_engine::grayscale(uint8_t* pixels, int width, int height, int stride, double amount) {
    double m[20];
    for (int i = 0; i < 20; ++i) {
        m[i] = filter_matrices::GRAYSCALE_MATRIX[i] * amount + (i % 5 == 4 ? 0 : (i < 15 ? (1 - amount) * (i % 5 == i / 5 ? 1 : 0) : (i % 5 == 3 ? 1 - amount + amount * filter_matrices::GRAYSCALE_MATRIX[i] : 0)));
    }
    if (amount >= 1.0) return apply_color_matrix(pixels, width, height, stride, filter_matrices::GRAYSCALE_MATRIX);
    return apply_color_matrix(pixels, width, height, stride, m);
}

bool filter_engine::sepia(uint8_t* pixels, int width, int height, int stride, double amount) {
    if (amount >= 1.0) return apply_color_matrix(pixels, width, height, stride, filter_matrices::SEPIA_MATRIX);
    double m[20];
    for (int i = 0; i < 20; ++i) {
        m[i] = filter_matrices::SEPIA_MATRIX[i] * amount + (i % 5 == 4 ? 0 : (i < 15 ? (1 - amount) * (i % 5 == i / 5 ? 1 : 0) : (i % 5 == 3 ? 1 : 0)));
    }
    return apply_color_matrix(pixels, width, height, stride, m);
}

bool filter_engine::invert(uint8_t* pixels, int width, int height, int stride, double amount) {
    if (amount >= 1.0) return apply_color_matrix(pixels, width, height, stride, filter_matrices::INVERT_MATRIX);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            for (int c = 0; c < 3; ++c) {
                pixels[idx + c] = static_cast<uint8_t>((255 - pixels[idx + c]) * amount + pixels[idx + c] * (1 - amount));
            }
        }
    }
    return true;
}

bool filter_engine::saturate(uint8_t* pixels, int width, int height, int stride, double amount) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            double r = pixels[idx] / 255.0, g = pixels[idx + 1] / 255.0, b = pixels[idx + 2] / 255.0;
            double gray = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            r = gray + amount * (r - gray);
            g = gray + amount * (g - gray);
            b = gray + amount * (b - gray);
            pixels[idx] = static_cast<uint8_t>(std::clamp(r * 255.0, 0.0, 255.0));
            pixels[idx + 1] = static_cast<uint8_t>(std::clamp(g * 255.0, 0.0, 255.0));
            pixels[idx + 2] = static_cast<uint8_t>(std::clamp(b * 255.0, 0.0, 255.0));
        }
    }
    return true;
}

bool filter_engine::hue_rotate(uint8_t* pixels, int width, int height, int stride, double degrees) {
    double rad = degrees * 3.141592653589793 / 180.0;
    double c = std::cos(rad);
    double s = std::sin(rad);
    double m[20] = {
        0.213 + c * 0.787 - s * 0.213, 0.715 - c * 0.715 - s * 0.715, 0.072 - c * 0.072 + s * 0.928, 0, 0,
        0.213 - c * 0.213 + s * 0.143, 0.715 + c * 0.285 + s * 0.140, 0.072 - c * 0.072 - s * 0.283, 0, 0,
        0.213 - c * 0.213 - s * 0.787, 0.715 - c * 0.715 + s * 0.715, 0.072 + c * 0.928 + s * 0.072, 0, 0,
        0, 0, 0, 1, 0
    };
    return apply_color_matrix(pixels, width, height, stride, m);
}

bool filter_engine::opacity(uint8_t* pixels, int width, int height, int stride, double amount) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            pixels[idx + 3] = static_cast<uint8_t>(pixels[idx + 3] * amount);
        }
    }
    return true;
}

bool filter_engine::drop_shadow(uint8_t* pixels, int width, int height, int stride,
                                 double offset_x, double offset_y, double blur, uint32_t color) {
    (void)pixels; (void)width; (void)height; (void)stride;
    (void)offset_x; (void)offset_y; (void)blur; (void)color;
    return true;
}

bool filter_engine::apply_svg_filter(const svg_filter& filter,
                                      uint8_t* source, uint8_t* background,
                                      int width, int height, int stride,
                                      uint8_t* output) {
    filter_context ctx;
    ctx.width = width;
    ctx.height = height;
    ctx.stride = stride;
    ctx.source.assign(source, source + static_cast<size_t>(height) * stride);
    if (background) {
        ctx.background.assign(background, background + static_cast<size_t>(height) * stride);
    }

    for (const auto& prim : filter.primitives) {
        if (!apply_primitive(ctx, prim)) return false;
    }

    if (!ctx.primitive_results.empty()) {
        auto& last = ctx.primitive_results.rbegin()->second;
        std::memcpy(output, last.data(), last.size());
    } else {
        std::memcpy(output, source, static_cast<size_t>(height) * stride);
    }
    return true;
}

bool filter_engine::apply_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    switch (prim.type) {
        case svg_filter_primitive_type::FE_GAUSSIAN_BLUR:
            return apply_gaussian_blur_primitive(ctx, prim);
        case svg_filter_primitive_type::FE_COLOR_MATRIX:
            return apply_color_matrix_primitive(ctx, prim);
        case svg_filter_primitive_type::FE_OFFSET:
            return apply_offset_primitive(ctx, prim);
        case svg_filter_primitive_type::FE_FLOOD:
            return apply_flood_primitive(ctx, prim);
        case svg_filter_primitive_type::FE_MERGE:
            return apply_merge_primitive(ctx, prim);
        case svg_filter_primitive_type::FE_TURBULENCE:
            return apply_turbulence_primitive(ctx, prim);
        case svg_filter_primitive_type::FE_COMPOSITE:
        case svg_filter_primitive_type::FE_BLEND:
            return apply_composite_primitive(ctx, prim);
        default:
            return true;
    }
}

bool filter_engine::apply_gaussian_blur_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    auto* input = get_input_buffer(ctx, prim.input);
    if (!input) return false;

    double radius = prim.parameters.empty() ? 3.0 : prim.parameters[0];
    std::vector<uint8_t> result(input->size());
    std::memcpy(result.data(), input->data(), input->size());

    fast_blur(result.data(), ctx.width, ctx.height, ctx.stride, radius);
    ctx.primitive_results[prim.result] = std::move(result);
    return true;
}

bool filter_engine::apply_color_matrix_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    auto* input = get_input_buffer(ctx, prim.input);
    if (!input) return false;
    std::vector<uint8_t> result = *input;

    double m[20] = {};
    for (size_t i = 0; i < std::min(prim.color_matrix_values.size(), size_t(20)); ++i) {
        m[i] = prim.color_matrix_values[i];
    }
    apply_color_matrix(result.data(), ctx.width, ctx.height, ctx.stride, m);
    ctx.primitive_results[prim.result] = std::move(result);
    return true;
}

bool filter_engine::apply_offset_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    auto* input = get_input_buffer(ctx, prim.input);
    if (!input) return false;
    std::vector<uint8_t> result(input->size());
    std::memcpy(result.data(), input->data(), input->size());
    ctx.primitive_results[prim.result] = std::move(result);
    return true;
}

bool filter_engine::apply_flood_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    std::vector<uint8_t> result(static_cast<size_t>(ctx.height) * ctx.stride, 0);
    uint32_t color = 0x00000000;
    if (prim.parameters.size() >= 4) {
        color = (static_cast<uint8_t>(std::clamp(prim.parameters[0] * 255.0, 0.0, 255.0)) << 24) |
                (static_cast<uint8_t>(std::clamp(prim.parameters[1] * 255.0, 0.0, 255.0)) << 16) |
                (static_cast<uint8_t>(std::clamp(prim.parameters[2] * 255.0, 0.0, 255.0)) << 8) |
                static_cast<uint8_t>(std::clamp(prim.parameters[3] * 255.0, 0.0, 255.0));
    }
    for (int y = 0; y < ctx.height; ++y) {
        for (int x = 0; x < ctx.width; ++x) {
            size_t idx = static_cast<size_t>(y) * ctx.stride + static_cast<size_t>(x) * 4;
            result[idx] = (color >> 24) & 0xFF;
            result[idx + 1] = (color >> 16) & 0xFF;
            result[idx + 2] = (color >> 8) & 0xFF;
            result[idx + 3] = color & 0xFF;
        }
    }
    ctx.primitive_results[prim.result] = std::move(result);
    return true;
}

bool filter_engine::apply_merge_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    auto* input = get_input_buffer(ctx, prim.input);
    if (!input) return false;
    ctx.primitive_results[prim.result] = *input;
    return true;
}

bool filter_engine::apply_turbulence_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    std::vector<uint8_t> result(static_cast<size_t>(ctx.height) * ctx.stride, 128);
    ctx.primitive_results[prim.result] = std::move(result);
    return true;
}

bool filter_engine::apply_composite_primitive(filter_context& ctx, const svg_filter_primitive& prim) {
    auto* input = get_input_buffer(ctx, prim.input);
    if (!input) return false;

    auto* in2 = get_input_buffer(ctx, prim.in2);
    if (!in2) {
        ctx.primitive_results[prim.result] = *input;
        return true;
    }

    std::vector<uint8_t> result(input->size());
    for (size_t i = 0; i < result.size(); i += 4) {
        float a1 = (*input)[i + 3] / 255.0f;
        float a2 = (*in2)[i + 3] / 255.0f;
        for (int c = 0; c < 4; ++c) {
            float v = (*input)[i + c] / 255.0f * a1 + (*in2)[i + c] / 255.0f * (1 - a1);
            result[i + c] = static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
        }
    }
    ctx.primitive_results[prim.result] = std::move(result);
    return true;
}

std::vector<uint8_t>* filter_engine::get_input_buffer(filter_context& ctx, const std::wstring& input_name) {
    if (input_name == L"SourceGraphic" || input_name.empty()) return &ctx.source;
    if (input_name == L"BackgroundImage" || input_name == L"BackgroundAlpha") return ctx.background.empty() ? nullptr : &ctx.background;
    auto it = ctx.primitive_results.find(input_name);
    return (it != ctx.primitive_results.end()) ? &it->second : nullptr;
}

} // namespace hre::css
