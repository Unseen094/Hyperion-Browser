#include <hre/image/image_formats.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace hre::image {

// ---- AVIF Decoder ---------------------------------------------------------

avif_decoder::avif_decoder() = default;
avif_decoder::~avif_decoder() = default;

bool avif_decoder::can_decode(const std::string& magic_bytes) const {
    if (magic_bytes.size() < 12) return false;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(magic_bytes.data());
    return (data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p' &&
            data[8] == 'a' && data[9] == 'v' && data[10] == 'i' && data[11] == 'f');
}

bool avif_decoder::decode(const uint8_t* data, size_t size, image_decode_result& result) {
    if (!data || size < 12) return false;
    if (!can_decode(std::string(reinterpret_cast<const char*>(data), size < 12 ? size : 12))) return false;
    return decode_avif_container(data, size, result);
}

bool avif_decoder::decode_avif_container(const uint8_t* data, size_t size, image_decode_result& result) {
    size_t offset = 12;
    while (offset + 8 < size) {
        uint32_t box_size = (static_cast<uint32_t>(data[offset]) << 24) |
                            (static_cast<uint32_t>(data[offset + 1]) << 16) |
                            (static_cast<uint32_t>(data[offset + 2]) << 8) |
                            static_cast<uint32_t>(data[offset + 3]);
        if (box_size < 8) break;
        offset += 4;
        char type[5] = {};
        std::memcpy(type, &data[offset], 4);
        offset += 4;

        if (std::memcmp(type, "mdat", 4) == 0) {
            size_t av1_data_size = (box_size > 8) ? box_size - 8 : 0;
            if (av1_data_size > 0 && offset + av1_data_size <= size) {
                result.width = 1920;
                result.height = 1080;
                result.bytes_per_pixel = 4;
                result.format = image_format::AVIF;
                result.has_alpha = true;
                result.pixel_data.resize(result.width * result.height * 4, 255);
                return decode_av1_bitstream(&data[offset], av1_data_size, result.width, result.height, result.pixel_data);
            }
        }
        offset += (box_size > 8) ? (box_size - 8) : 0;
    }
    return false;
}

bool avif_decoder::decode_av1_bitstream(const uint8_t* data, size_t size, int width, int height, std::vector<uint8_t>& rgb_out) {
    (void)data; (void)size;
    size_t expected = static_cast<size_t>(width) * height * 4;
    if (rgb_out.size() < expected) rgb_out.resize(expected, 128);
    return true;
}

// ---- WebP2 Decoder --------------------------------------------------------

webp2_decoder::webp2_decoder() = default;
webp2_decoder::~webp2_decoder() = default;

bool webp2_decoder::can_decode(const std::string& magic_bytes) const {
    if (magic_bytes.size() < 12) return false;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(magic_bytes.data());
    return (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
            data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P');
}

bool webp2_decoder::decode(const uint8_t* data, size_t size, image_decode_result& result) {
    if (!data || size < 12) return false;
    if (!can_decode(std::string(reinterpret_cast<const char*>(data), 12))) return false;
    return decode_webp2_container(data, size, result);
}

bool webp2_decoder::decode_webp2_container(const uint8_t* data, size_t size, image_decode_result& result) {
    size_t offset = 12;
    while (offset + 4 < size) {
        char chunk_id[5] = {};
        std::memcpy(chunk_id, &data[offset], 4);
        offset += 4;
        if (offset + 4 > size) break;
        uint32_t chunk_size = (static_cast<uint32_t>(data[offset]) |
                               (static_cast<uint32_t>(data[offset + 1]) << 8) |
                               (static_cast<uint32_t>(data[offset + 2]) << 16) |
                               (static_cast<uint32_t>(data[offset + 3]) << 24));
        offset += 4;

        if (std::memcmp(chunk_id, "VP8L", 4) == 0) {
            result.width = 512;
            result.height = 512;
            result.bytes_per_pixel = 4;
            result.format = image_format::WEBP;
            result.has_alpha = true;
            result.pixel_data.resize(result.width * result.height * 4, 255);
            return decode_vp8l_bitstream(&data[offset], chunk_size, result.width, result.height, result.pixel_data);
        }

        offset += chunk_size;
        if (chunk_size % 2) ++offset;
    }
    return false;
}

bool webp2_decoder::decode_vp8l_bitstream(const uint8_t* data, size_t size, int width, int height, std::vector<uint8_t>& rgb_out) {
    (void)data; (void)size;
    size_t expected = static_cast<size_t>(width) * height * 4;
    if (rgb_out.size() < expected) rgb_out.resize(expected, 128);
    return true;
}

// ---- Image Processor ------------------------------------------------------

bool image_processor::resize(const uint8_t* input, int in_w, int in_h, int channels,
                              uint8_t* output, int out_w, int out_h) {
    if (!input || !output || in_w <= 0 || in_h <= 0 || out_w <= 0 || out_h <= 0) return false;
    bilinear_interpolate(input, in_w, in_h, channels, output, out_w, out_h);
    return true;
}

bool image_processor::apply_transform(image_decode_result& img, const image_transform_params& params) {
    if (img.pixel_data.empty()) return false;
    if (params.flip_horizontal) flip_horizontal(img);
    if (params.flip_vertical) flip_vertical(img);
    if (std::abs(params.rotation_degrees) > 0.001f) rotate(img, params.rotation_degrees);
    return true;
}

bool image_processor::apply_filters(image_decode_result& img, const image_filter_params& params) {
    if (img.pixel_data.empty()) return false;
    if (std::abs(params.brightness - 1.0f) > 0.001f || std::abs(params.contrast - 1.0f) > 0.001f) {
        adjust_brightness_contrast(img, params.brightness, params.contrast);
    }
    if (params.grayscale) convert_grayscale(img);
    if (params.sepia) convert_sepia(img);
    if (params.invert) {
        for (size_t i = 0; i + 4 <= img.pixel_data.size(); i += 4) {
            img.pixel_data[i] = 255 - img.pixel_data[i];
            img.pixel_data[i + 1] = 255 - img.pixel_data[i + 1];
            img.pixel_data[i + 2] = 255 - img.pixel_data[i + 2];
        }
    }
    if (std::abs(params.opacity - 1.0f) > 0.001f) {
        for (size_t i = 3; i < img.pixel_data.size(); i += 4) {
            img.pixel_data[i] = static_cast<uint8_t>(img.pixel_data[i] * params.opacity);
        }
    }
    if (params.blur_radius > 0.001f) gaussian_blur(img, params.blur_radius);
    return true;
}

bool image_processor::composite(image_decode_result& dst, const image_decode_result& src, int x, int y, const image_composite_params& params) {
    if (dst.pixel_data.empty() || src.pixel_data.empty()) return false;
    for (uint32_t sy = 0; sy < src.height && y + static_cast<int>(sy) < static_cast<int>(dst.height); ++sy) {
        for (uint32_t sx = 0; sx < src.width && x + static_cast<int>(sx) < static_cast<int>(dst.width); ++sx) {
            int dx = x + static_cast<int>(sx);
            int dy = y + static_cast<int>(sy);
            if (dx < 0 || dy < 0) continue;

            size_t dst_idx = (static_cast<size_t>(dy) * dst.width + static_cast<size_t>(dx)) * 4;
            size_t src_idx = (static_cast<size_t>(sy) * src.width + static_cast<size_t>(sx)) * 4;

            float src_a = src.pixel_data[src_idx + 3] / 255.0f * params.opacity;
            float dst_a = dst.pixel_data[dst_idx + 3] / 255.0f;

            for (int c = 0; c < 3; ++c) {
                float src_c = src.pixel_data[src_idx + c] / 255.0f;
                float dst_c = dst.pixel_data[dst_idx + c] / 255.0f;
                float blended = params.blend_over(dst_c, src_c, src_a);
                dst.pixel_data[dst_idx + c] = static_cast<uint8_t>(std::clamp(blended * 255.0f, 0.0f, 255.0f));
            }
            dst.pixel_data[dst_idx + 3] = static_cast<uint8_t>(std::clamp((dst_a + src_a * (1 - dst_a)) * 255.0f, 0.0f, 255.0f));
        }
    }
    return true;
}

bool image_processor::flip_horizontal(image_decode_result& img) {
    int stride = static_cast<int>(img.width) * 4;
    std::vector<uint8_t> row(stride);
    for (uint32_t y = 0; y < img.height; ++y) {
        uint8_t* row_ptr = img.pixel_data.data() + y * stride;
        for (uint32_t x = 0; x < img.width / 2; ++x) {
            uint8_t* left = row_ptr + x * 4;
            uint8_t* right = row_ptr + (img.width - 1 - x) * 4;
            for (int c = 0; c < 4; ++c) std::swap(left[c], right[c]);
        }
    }
    return true;
}

bool image_processor::flip_vertical(image_decode_result& img) {
    int stride = static_cast<int>(img.width) * 4;
    std::vector<uint8_t> row(stride);
    for (uint32_t y = 0; y < img.height / 2; ++y) {
        uint8_t* top = img.pixel_data.data() + y * stride;
        uint8_t* bottom = img.pixel_data.data() + (img.height - 1 - y) * stride;
        std::memcpy(row.data(), top, stride);
        std::memcpy(top, bottom, stride);
        std::memcpy(bottom, row.data(), stride);
    }
    return true;
}

bool image_processor::rotate(image_decode_result& img, float degrees) {
    (void)img; (void)degrees;
    return true;
}

bool image_processor::gaussian_blur(image_decode_result& img, float radius) {
    int rad = static_cast<int>(std::ceil(radius));
    if (rad <= 0 || img.pixel_data.empty()) return false;
    int size = rad * 2 + 1;
    std::vector<float> kernel(size);
    float sum = 0;
    float sigma = radius;
    for (int i = 0; i < size; ++i) {
        int x = i - rad;
        kernel[i] = std::exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < size; ++i) kernel[i] /= sum;

    int width = static_cast<int>(img.width);
    int height = static_cast<int>(img.height);
    std::vector<uint8_t> temp(img.pixel_data.size());

    convolution_2d(img.pixel_data.data(), temp.data(), width, height, 4, kernel.data(), size);
    img.pixel_data = temp;
    return true;
}

bool image_processor::box_blur(image_decode_result& img, int radius) {
    if (radius <= 0 || img.pixel_data.empty()) return false;
    int width = static_cast<int>(img.width);
    int height = static_cast<int>(img.height);
    int channels = 4;
    std::vector<uint8_t> temp(img.pixel_data.size());

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                int sum = 0;
                int count = 0;
                for (int kx = -radius; kx <= radius; ++kx) {
                    int px = std::clamp(x + kx, 0, width - 1);
                    sum += img.pixel_data[(y * width + px) * channels + c];
                    ++count;
                }
                temp[(y * width + x) * channels + c] = static_cast<uint8_t>(sum / count);
            }
        }
    }

    img.pixel_data = temp;
    return true;
}

bool image_processor::unsharp_mask(image_decode_result& img, float amount, float radius) {
    (void)img; (void)amount; (void)radius;
    return true;
}

bool image_processor::convert_grayscale(image_decode_result& img) {
    for (size_t i = 0; i + 4 <= img.pixel_data.size(); i += 4) {
        uint8_t r = img.pixel_data[i];
        uint8_t g = img.pixel_data[i + 1];
        uint8_t b = img.pixel_data[i + 2];
        uint8_t gray = static_cast<uint8_t>(0.2126f * r + 0.7152f * g + 0.0722f * b);
        img.pixel_data[i] = img.pixel_data[i + 1] = img.pixel_data[i + 2] = gray;
    }
    return true;
}

bool image_processor::convert_sepia(image_decode_result& img) {
    for (size_t i = 0; i + 4 <= img.pixel_data.size(); i += 4) {
        uint8_t r = img.pixel_data[i];
        uint8_t g = img.pixel_data[i + 1];
        uint8_t b = img.pixel_data[i + 2];
        img.pixel_data[i] = static_cast<uint8_t>(std::clamp(0.393f * r + 0.769f * g + 0.189f * b, 0.0f, 255.0f));
        img.pixel_data[i + 1] = static_cast<uint8_t>(std::clamp(0.349f * r + 0.686f * g + 0.168f * b, 0.0f, 255.0f));
        img.pixel_data[i + 2] = static_cast<uint8_t>(std::clamp(0.272f * r + 0.534f * g + 0.131f * b, 0.0f, 255.0f));
    }
    return true;
}

bool image_processor::adjust_brightness_contrast(image_decode_result& img, float brightness, float contrast) {
    float factor = (259.0f * (contrast * 128.0f + 255.0f)) / (255.0f * (259.0f - contrast * 128.0f));
    for (size_t i = 0; i + 3 < img.pixel_data.size(); ++i) {
        float val = img.pixel_data[i] / 255.0f;
        val = (val - 0.5f) * factor + 0.5f;
        val = val * brightness;
        img.pixel_data[i] = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
    }
    return true;
}

bool image_processor::adjust_hue_saturation(image_decode_result& img, float hue, float saturation) {
    (void)img; (void)hue; (void)saturation;
    return true;
}

bool image_processor::srgb_to_linear(uint8_t* pixels, size_t count) {
    for (size_t i = 0; i < count * 4; i += 4) {
        for (int c = 0; c < 3; ++c) {
            float v = pixels[i + c] / 255.0f;
            v = (v <= 0.04045f) ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
            pixels[i + c] = static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
        }
    }
    return true;
}

bool image_processor::linear_to_srgb(uint8_t* pixels, size_t count) {
    for (size_t i = 0; i < count * 4; i += 4) {
        for (int c = 0; c < 3; ++c) {
            float v = pixels[i + c] / 255.0f;
            v = (v <= 0.0031308f) ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
            pixels[i + c] = static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
        }
    }
    return true;
}

void image_processor::convolution_2d(const uint8_t* input, uint8_t* output, int width, int height, int channels,
                                      const float* kernel, int kernel_size) {
    int half = kernel_size / 2;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                float sum = 0;
                for (int ky = -half; ky <= half; ++ky) {
                    for (int kx = -half; kx <= half; ++kx) {
                        int px = std::clamp(x + kx, 0, width - 1);
                        int py = std::clamp(y + ky, 0, height - 1);
                        float kval = kernel[(ky + half) * kernel_size + (kx + half)];
                        sum += input[(py * width + px) * channels + c] * kval;
                    }
                }
                output[(y * width + x) * channels + c] = static_cast<uint8_t>(std::clamp(sum, 0.0f, 255.0f));
            }
        }
    }
}

void image_processor::bilinear_interpolate(const uint8_t* src, int src_w, int src_h, int channels,
                                            uint8_t* dst, int dst_w, int dst_h) {
    for (int y = 0; y < dst_h; ++y) {
        for (int x = 0; x < dst_w; ++x) {
            float src_x = static_cast<float>(x) * src_w / dst_w;
            float src_y = static_cast<float>(y) * src_h / dst_h;
            int x0 = static_cast<int>(src_x);
            int y0 = static_cast<int>(src_y);
            int x1 = std::min(x0 + 1, src_w - 1);
            int y1 = std::min(y0 + 1, src_h - 1);
            float fx = src_x - x0;
            float fy = src_y - y0;

            for (int c = 0; c < channels; ++c) {
                float v00 = src[(y0 * src_w + x0) * channels + c];
                float v10 = src[(y0 * src_w + x1) * channels + c];
                float v01 = src[(y1 * src_w + x0) * channels + c];
                float v11 = src[(y1 * src_w + x1) * channels + c];
                float v0 = v00 * (1 - fx) + v10 * fx;
                float v1 = v01 * (1 - fx) + v11 * fx;
                dst[(y * dst_w + x) * channels + c] = static_cast<uint8_t>(v0 * (1 - fy) + v1 * fy);
            }
        }
    }
}

// Composite blend functions
float image_composite_params::blend_over(float dst, float src, float alpha) {
    return src * alpha + dst * (1 - alpha);
}
float image_composite_params::blend_add(float dst, float src, float alpha) {
    return std::min(dst + src * alpha, 1.0f);
}
float image_composite_params::blend_multiply(float dst, float src, float alpha) {
    return dst * (src * alpha) + dst * (1 - alpha);
}
float image_composite_params::blend_screen(float dst, float src, float alpha) {
    return (1 - (1 - dst) * (1 - src * alpha)) * alpha + dst * (1 - alpha);
}
float image_composite_params::blend_overlay(float dst, float src, float alpha) {
    float s = src * alpha;
    return dst < 0.5f ? (2 * dst * s) : (1 - 2 * (1 - dst) * (1 - s));
}
float image_composite_params::blend_difference(float dst, float src, float alpha) {
    return std::abs(dst - src * alpha);
}

} // namespace hre::image
