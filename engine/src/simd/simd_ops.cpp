#include <hre\simd\simd_ops.hpp>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>

#if defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#endif

namespace hre::simd {

simd_level get_supported_simd_level() {
#if defined(__AVX512F__)
    return simd_level::AVX512;
#elif defined(__AVX2__)
    return simd_level::AVX2;
#elif defined(__AVX__)
    return simd_level::AVX;
#elif defined(__SSE4_1__)
    return simd_level::SSE4;
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86) && defined(_M_IX86_FP))
    return simd_level::SSE2;
#else
    return simd_level::NONE;
#endif
}

void* alloc_aligned(size_t size, size_t alignment) {
    void* ptr = nullptr;
#if defined(_MSC_VER)
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return nullptr;
    }
#endif
    return ptr;
}

void free_aligned(void* ptr) {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void memcpy_simd(void* dest, const void* src, size_t size) {
    std::memcpy(dest, src, size);
}

void memset_simd(void* dest, uint8_t value, size_t size) {
    std::memset(dest, value, size);
}

float hadd_float32x4(const float32x4& a) {
    return a.data[0] + a.data[1] + a.data[2] + a.data[3];
}

float hadd_float32x8(const float32x8& a) {
    return a.data[0] + a.data[1] + a.data[2] + a.data[3] +
           a.data[4] + a.data[5] + a.data[6] + a.data[7];
}

float32x4 add_float32x4(const float32x4& a, const float32x4& b) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] + b.data[i];
    }
    return result;
}

float32x4 sub_float32x4(const float32x4& a, const float32x4& b) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] - b.data[i];
    }
    return result;
}

float32x4 mul_float32x4(const float32x4& a, const float32x4& b) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] * b.data[i];
    }
    return result;
}

float32x4 div_float32x4(const float32x4& a, const float32x4& b) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] / b.data[i];
    }
    return result;
}

float32x4 min_float32x4(const float32x4& a, const float32x4& b) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = std::min(a.data[i], b.data[i]);
    }
    return result;
}

float32x4 max_float32x4(const float32x4& a, const float32x4& b) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = std::max(a.data[i], b.data[i]);
    }
    return result;
}

float32x4 sqrt_float32x4(const float32x4& a) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = std::sqrt(a.data[i]);
    }
    return result;
}

float32x4 rsqrt_float32x4(const float32x4& a) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = 1.0f / std::sqrt(a.data[i]);
    }
    return result;
}

float32x4 load_float32x4(const float* ptr) {
    float32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

void store_float32x4(float* ptr, const float32x4& a) {
    for (int i = 0; i < 4; ++i) {
        ptr[i] = a.data[i];
    }
}

void layout_flex_sizing_simd(const float* grow_values,
                              const float* shrink_values,
                              const float* basis_values,
                              float* result_main_sizes,
                              size_t item_count,
                              float available_space,
                              bool is_row) {
    for (size_t i = 0; i < item_count; ++i) {
        float size = basis_values[i];
        if (is_row) {
            size += grow_values[i] * 10.0f;
        } else {
            size += shrink_values[i] * 5.0f;
        }
        result_main_sizes[i] = size;
    }
}

void layout_grid_track_sizing_simd(const float* min_sizes,
                                    const float* max_sizes,
                                    float* track_sizes,
                                    size_t track_count,
                                    float available_space) {
    for (size_t i = 0; i < track_count; ++i) {
        track_sizes[i] = (min_sizes[i] + max_sizes[i]) * 0.5f;
    }
}

void image_blur_row_simd(uint8_t* dest,
                         const uint8_t* src,
                         int width,
                         int height,
                         int bytes_per_pixel,
                         float blur_radius) {
    int radius = static_cast<int>(blur_radius);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            int count = 0;
            for (int k = -radius; k <= radius; ++k) {
                int nx = std::max(0, std::min(width - 1, x + k));
                for (int c = 0; c < bytes_per_pixel; ++c) {
                    sum += src[(y * width + nx) * bytes_per_pixel + c];
                    ++count;
                }
            }
            for (int c = 0; c < bytes_per_pixel; ++c) {
                dest[(y * width + x) * bytes_per_pixel + c] = static_cast<uint8_t>(sum / count);
            }
        }
    }
}

void image_blur_col_simd(uint8_t* dest,
                         const uint8_t* src,
                         int width,
                         int height,
                         int bytes_per_pixel,
                         float blur_radius) {
    int radius = static_cast<int>(blur_radius);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            float sum = 0.0f;
            int count = 0;
            for (int k = -radius; k <= radius; ++k) {
                int ny = std::max(0, std::min(height - 1, y + k));
                for (int c = 0; c < bytes_per_pixel; ++c) {
                    sum += src[(ny * width + x) * bytes_per_pixel + c];
                    ++count;
                }
            }
            for (int c = 0; c < bytes_per_pixel; ++c) {
                dest[(y * width + x) * bytes_per_pixel + c] = static_cast<uint8_t>(sum / count);
            }
        }
    }
}

void matrix4x4_transform_point(const float* matrix,
                                const float* points_in,
                                float* points_out,
                                size_t point_count) {
    for (size_t i = 0; i < point_count; ++i) {
        float x = points_in[i * 3];
        float y = points_in[i * 3 + 1];
        float z = points_in[i * 3 + 2];
        points_out[i * 3] = matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12];
        points_out[i * 3 + 1] = matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13];
        points_out[i * 3 + 2] = matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14];
    }
}

// ---- Phase 30: Graphics Blitting Operations (SIMD) -----------------------

// Helper: clamp float to [0,255]
static inline uint8_t clamp_uint8(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
}

void blend_pixels_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        float src_a = src[idx + 3] / 255.0f;
        float dst_a = dest[idx + 3] / 255.0f;
        float out_a = src_a + dst_a * (1.0f - src_a);
        if (out_a > 0) {
            for (int c = 0; c < 3; ++c) {
                float v = (src[idx + c] * src_a + dest[idx + c] * dst_a * (1.0f - src_a)) / out_a;
                dest[idx + c] = clamp_uint8(v);
            }
        }
        dest[idx + 3] = clamp_uint8(out_a * 255.0f);
    }
}

void blend_pixels_simd_premultiplied(uint8_t* dest, const uint8_t* src, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        float src_a = src[idx + 3] / 255.0f;
        float dst_a = dest[idx + 3] / 255.0f;
        float out_a = src_a + dst_a * (1.0f - src_a);
        if (out_a > 0) {
            for (int c = 0; c < 3; ++c) {
                float v = src[idx + c] + dest[idx + c] * (1.0f - src_a);
                dest[idx + c] = clamp_uint8(v);
            }
        }
        dest[idx + 3] = clamp_uint8(out_a * 255.0f);
    }
}

void blit_image_simd(uint8_t* dest, int dest_stride, int dest_x, int dest_y,
                     const uint8_t* src, int src_stride, int src_w, int src_h,
                     int src_x, int src_y, int blit_w, int blit_h) {
    for (int sy = 0; sy < blit_h; ++sy) {
        int dy = dest_y + sy;
        int ssy = src_y + sy;
        if (dy < 0 || ssy >= src_h || ssy < 0) continue;
        for (int sx = 0; sx < blit_w; ++sx) {
            int dx = dest_x + sx;
            int ssx = src_x + sx;
            if (dx < 0 || ssx >= src_w || ssx < 0) continue;
            size_t dst_idx = static_cast<size_t>(dy) * dest_stride + static_cast<size_t>(dx) * 4;
            size_t src_idx = static_cast<size_t>(ssy) * src_stride + static_cast<size_t>(ssx) * 4;
            std::memcpy(&dest[dst_idx], &src[src_idx], 4);
        }
    }
}

void blit_image_scaled_simd(uint8_t* dest, int dest_stride, int dest_w, int dest_h,
                            const uint8_t* src, int src_stride, int src_w, int src_h) {
    for (int dy = 0; dy < dest_h; ++dy) {
        for (int dx = 0; dx < dest_w; ++dx) {
            float sx = static_cast<float>(dx) * src_w / dest_w;
            float sy = static_cast<float>(dy) * src_h / dest_h;
            int ix0 = static_cast<int>(sx), iy0 = static_cast<int>(sy);
            int ix1 = std::min(ix0 + 1, src_w - 1);
            int iy1 = std::min(iy0 + 1, src_h - 1);
            float fx = sx - ix0, fy = sy - iy0;

            size_t dst_idx = static_cast<size_t>(dy) * dest_stride + static_cast<size_t>(dx) * 4;
            for (int c = 0; c < 4; ++c) {
                float v00 = src[static_cast<size_t>(iy0) * src_stride + ix0 * 4 + c];
                float v10 = src[static_cast<size_t>(iy0) * src_stride + ix1 * 4 + c];
                float v01 = src[static_cast<size_t>(iy1) * src_stride + ix0 * 4 + c];
                float v11 = src[static_cast<size_t>(iy1) * src_stride + ix1 * 4 + c];
                float v0 = v00 * (1 - fx) + v10 * fx;
                float v1 = v01 * (1 - fx) + v11 * fx;
                dest[dst_idx + c] = clamp_uint8(v0 * (1 - fy) + v1 * fy);
            }
        }
    }
}

void fill_rect_simd(uint8_t* pixels, int stride, int x, int y, int w, int h, uint32_t color) {
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            size_t idx = static_cast<size_t>(y + row) * stride + static_cast<size_t>(x + col) * 4;
            pixels[idx] = (color >> 24) & 0xFF;
            pixels[idx + 1] = (color >> 16) & 0xFF;
            pixels[idx + 2] = (color >> 8) & 0xFF;
            pixels[idx + 3] = color & 0xFF;
        }
    }
}

void fill_rect_gradient_horizontal_simd(uint8_t* pixels, int stride, int x, int y, int w, int h,
                                        uint32_t color_left, uint32_t color_right) {
    uint8_t lr[4] = { static_cast<uint8_t>((color_left >> 24) & 0xFF), static_cast<uint8_t>((color_left >> 16) & 0xFF),
                      static_cast<uint8_t>((color_left >> 8) & 0xFF), static_cast<uint8_t>(color_left & 0xFF) };
    uint8_t rr[4] = { static_cast<uint8_t>((color_right >> 24) & 0xFF), static_cast<uint8_t>((color_right >> 16) & 0xFF),
                      static_cast<uint8_t>((color_right >> 8) & 0xFF), static_cast<uint8_t>(color_right & 0xFF) };
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            float t = w > 1 ? static_cast<float>(col) / (w - 1) : 0;
            size_t idx = static_cast<size_t>(y + row) * stride + static_cast<size_t>(x + col) * 4;
            for (int c = 0; c < 4; ++c)
                pixels[idx + c] = clamp_uint8(lr[c] * (1 - t) + rr[c] * t);
        }
    }
}

void fill_rect_gradient_vertical_simd(uint8_t* pixels, int stride, int x, int y, int w, int h,
                                      uint32_t color_top, uint32_t color_bottom) {
    uint8_t tc[4] = { static_cast<uint8_t>((color_top >> 24) & 0xFF), static_cast<uint8_t>((color_top >> 16) & 0xFF),
                      static_cast<uint8_t>((color_top >> 8) & 0xFF), static_cast<uint8_t>(color_top & 0xFF) };
    uint8_t bc[4] = { static_cast<uint8_t>((color_bottom >> 24) & 0xFF), static_cast<uint8_t>((color_bottom >> 16) & 0xFF),
                      static_cast<uint8_t>((color_bottom >> 8) & 0xFF), static_cast<uint8_t>(color_bottom & 0xFF) };
    for (int row = 0; row < h; ++row) {
        float t = h > 1 ? static_cast<float>(row) / (h - 1) : 0;
        for (int col = 0; col < w; ++col) {
            size_t idx = static_cast<size_t>(y + row) * stride + static_cast<size_t>(x + col) * 4;
            for (int c = 0; c < 4; ++c)
                pixels[idx + c] = clamp_uint8(tc[c] * (1 - t) + bc[c] * t);
        }
    }
}

void convert_rgba_to_bgra_simd(uint8_t* pixels, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        std::swap(pixels[idx], pixels[idx + 2]);
    }
}

void convert_rgba_to_premultiplied_simd(uint8_t* pixels, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        float a = pixels[idx + 3] / 255.0f;
        if (a < 1.0f) {
            pixels[idx] = clamp_uint8(pixels[idx] * a);
            pixels[idx + 1] = clamp_uint8(pixels[idx + 1] * a);
            pixels[idx + 2] = clamp_uint8(pixels[idx + 2] * a);
        }
    }
}

void convert_premultiplied_to_rgba_simd(uint8_t* pixels, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        float a = pixels[idx + 3] / 255.0f;
        if (a > 0 && a < 1.0f) {
            pixels[idx] = clamp_uint8(pixels[idx] / a);
            pixels[idx + 1] = clamp_uint8(pixels[idx + 1] / a);
            pixels[idx + 2] = clamp_uint8(pixels[idx + 2] / a);
        }
    }
}

void apply_color_matrix_simd(uint8_t* pixels, size_t pixel_count, const double matrix[20]) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        double r = pixels[idx] / 255.0, g = pixels[idx + 1] / 255.0;
        double b = pixels[idx + 2] / 255.0, a = pixels[idx + 3] / 255.0;
        pixels[idx] = clamp_uint8(static_cast<float>((matrix[0]*r + matrix[1]*g + matrix[2]*b + matrix[3]*a + matrix[4]) * 255.0));
        pixels[idx + 1] = clamp_uint8(static_cast<float>((matrix[5]*r + matrix[6]*g + matrix[7]*b + matrix[8]*a + matrix[9]) * 255.0));
        pixels[idx + 2] = clamp_uint8(static_cast<float>((matrix[10]*r + matrix[11]*g + matrix[12]*b + matrix[13]*a + matrix[14]) * 255.0));
        pixels[idx + 3] = clamp_uint8(static_cast<float>((matrix[15]*r + matrix[16]*g + matrix[17]*b + matrix[18]*a + matrix[19]) * 255.0));
    }
}

void composite_over_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count) {
    blend_pixels_simd(dest, src, pixel_count);
}

void composite_add_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        for (int c = 0; c < 4; ++c)
            dest[idx + c] = clamp_uint8(dest[idx + c] + src[idx + c]);
    }
}

void composite_multiply_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        for (int c = 0; c < 3; ++c)
            dest[idx + c] = clamp_uint8(dest[idx + c] * src[idx + c] / 255.0f);
    }
}

void composite_screen_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        for (int c = 0; c < 3; ++c)
            dest[idx + c] = clamp_uint8(255 - (255 - dest[idx + c]) * (255 - src[idx + c]) / 255.0f);
    }
}

void adjust_brightness_simd(uint8_t* pixels, size_t pixel_count, float brightness) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        for (int c = 0; c < 3; ++c)
            pixels[idx + c] = clamp_uint8(pixels[idx + c] * brightness);
    }
}

void adjust_contrast_simd(uint8_t* pixels, size_t pixel_count, float contrast) {
    float factor = (259.0f * (contrast * 128.0f + 255.0f)) / (255.0f * (259.0f - contrast * 128.0f));
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        for (int c = 0; c < 3; ++c) {
            float v = pixels[idx + c] / 255.0f;
            v = (v - 0.5f) * factor + 0.5f;
            pixels[idx + c] = clamp_uint8(v * 255.0f);
        }
    }
}

void adjust_saturation_simd(uint8_t* pixels, size_t pixel_count, float saturation) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        float r = pixels[idx] / 255.0f, g = pixels[idx + 1] / 255.0f, b = pixels[idx + 2] / 255.0f;
        float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        pixels[idx] = clamp_uint8((gray + saturation * (r - gray)) * 255.0f);
        pixels[idx + 1] = clamp_uint8((gray + saturation * (g - gray)) * 255.0f);
        pixels[idx + 2] = clamp_uint8((gray + saturation * (b - gray)) * 255.0f);
    }
}

void adjust_opacity_simd(uint8_t* pixels, size_t pixel_count, float opacity) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        pixels[idx + 3] = clamp_uint8(pixels[idx + 3] * opacity);
    }
}

void invert_colors_simd(uint8_t* pixels, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        pixels[idx] = 255 - pixels[idx];
        pixels[idx + 1] = 255 - pixels[idx + 1];
        pixels[idx + 2] = 255 - pixels[idx + 2];
    }
}

void blur_horizontal_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, float radius) {
    int r = static_cast<int>(std::ceil(radius));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum[4] = {};
            int count = 0;
            for (int kx = -r; kx <= r; ++kx) {
                int px = std::clamp(x + kx, 0, width - 1);
                size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(px) * 4;
                for (int c = 0; c < 4; ++c) sum[c] += src[idx + c];
                ++count;
            }
            size_t d_idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            for (int c = 0; c < 4; ++c) dest[d_idx + c] = clamp_uint8(sum[c] / count);
        }
    }
}

void blur_vertical_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, float radius) {
    int r = static_cast<int>(std::ceil(radius));
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            float sum[4] = {};
            int count = 0;
            for (int ky = -r; ky <= r; ++ky) {
                int py = std::clamp(y + ky, 0, height - 1);
                size_t idx = static_cast<size_t>(py) * stride + static_cast<size_t>(x) * 4;
                for (int c = 0; c < 4; ++c) sum[c] += src[idx + c];
                ++count;
            }
            size_t d_idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            for (int c = 0; c < 4; ++c) dest[d_idx + c] = clamp_uint8(sum[c] / count);
        }
    }
}

void box_blur_horizontal_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, int radius) {
    blur_horizontal_simd(dest, src, width, height, stride, static_cast<float>(radius));
}

void box_blur_vertical_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, int radius) {
    blur_vertical_simd(dest, src, width, height, stride, static_cast<float>(radius));
}

void apply_alpha_mask_simd(uint8_t* pixels, size_t pixel_count, const uint8_t* mask) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        float m = mask[i] / 255.0f;
        pixels[idx + 3] = clamp_uint8(pixels[idx + 3] * m);
    }
}

void apply_luminance_to_alpha_simd(uint8_t* pixels, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        size_t idx = i * 4;
        float lum = 0.2126f * pixels[idx] + 0.7152f * pixels[idx + 1] + 0.0722f * pixels[idx + 2];
        pixels[idx + 3] = clamp_uint8(lum);
    }
}

uint8x16 load_uint8x16(const uint8_t* ptr) {
    uint8x16 r;
    std::memcpy(r.data, ptr, 16);
    return r;
}

void store_uint8x16(uint8_t* ptr, const uint8x16& v) {
    std::memcpy(ptr, v.data, 16);
}

uint8x16 blend_alpha_uint8x16(const uint8x16& dest, const uint8x16& src) {
    uint8x16 result;
    for (int i = 0; i < 4; ++i) {
        int base = i * 4;
        float src_a = src.data[base + 3] / 255.0f;
        float dst_a = dest.data[base + 3] / 255.0f;
        float out_a = src_a + dst_a * (1.0f - src_a);
        for (int c = 0; c < 3; ++c) {
            float v = out_a > 0 ? (src.data[base + c] * src_a + dest.data[base + c] * dst_a * (1.0f - src_a)) / out_a : 0;
            result.data[base + c] = clamp_uint8(v);
        }
        result.data[base + 3] = clamp_uint8(out_a * 255.0f);
    }
    return result;
}

} // namespace hre::simd