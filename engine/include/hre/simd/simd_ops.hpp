#pragma once

#include <cstdint>
#include <cstddef>

namespace hre::simd {

enum class simd_level {
    NONE,
    SSE2,
    SSE4,
    AVX,
    AVX2,
    AVX512
};

simd_level get_supported_simd_level();

struct float32x4 {
    float data[4];
};

struct float32x8 {
    float data[8];
};

struct int32x4 {
    int32_t data[4];
};

struct int32x8 {
    int32_t data[8];
};

struct uint8x16 {
    uint8_t data[16];
};

struct uint8x32 {
    uint8_t data[32];
};

void* alloc_aligned(size_t size, size_t alignment);
void free_aligned(void* ptr);

void memcpy_simd(void* dest, const void* src, size_t size);
void memset_simd(void* dest, uint8_t value, size_t size);

float hadd_float32x4(const float32x4& a);
float hadd_float32x8(const float32x8& a);

float32x4 add_float32x4(const float32x4& a, const float32x4& b);
float32x4 sub_float32x4(const float32x4& a, const float32x4& b);
float32x4 mul_float32x4(const float32x4& a, const float32x4& b);
float32x4 div_float32x4(const float32x4& a, const float32x4& b);

float32x4 min_float32x4(const float32x4& a, const float32x4& b);
float32x4 max_float32x4(const float32x4& a, const float32x4& b);

float32x4 sqrt_float32x4(const float32x4& a);
float32x4 rsqrt_float32x4(const float32x4& a);

float32x4 load_float32x4(const float* ptr);
void store_float32x4(float* ptr, const float32x4& a);

// ---- Graphics Blitting Operations (Phase 30) -----------------------------

// Premultiplied alpha blending of two RGBA pixels (SIMD)
void blend_pixels_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count);
void blend_pixels_simd_premultiplied(uint8_t* dest, const uint8_t* src, size_t pixel_count);

// Image blitting with alpha blending
void blit_image_simd(uint8_t* dest, int dest_stride, int dest_x, int dest_y,
                     const uint8_t* src, int src_stride, int src_w, int src_h,
                     int src_x, int src_y, int blit_w, int blit_h);

// Scaled blit with bilinear interpolation (SIMD)
void blit_image_scaled_simd(uint8_t* dest, int dest_stride, int dest_w, int dest_h,
                            const uint8_t* src, int src_stride, int src_w, int src_h);

// Fill rectangle with color (SIMD)
void fill_rect_simd(uint8_t* pixels, int stride, int x, int y, int w, int h, uint32_t color);

// Fill rectangle with gradient (SIMD)
void fill_rect_gradient_horizontal_simd(uint8_t* pixels, int stride, int x, int y, int w, int h,
                                        uint32_t color_left, uint32_t color_right);
void fill_rect_gradient_vertical_simd(uint8_t* pixels, int stride, int x, int y, int w, int h,
                                      uint32_t color_top, uint32_t color_bottom);

// Pixel format conversion (SIMD)
void convert_rgba_to_bgra_simd(uint8_t* pixels, size_t pixel_count);
void convert_rgba_to_premultiplied_simd(uint8_t* pixels, size_t pixel_count);
void convert_premultiplied_to_rgba_simd(uint8_t* pixels, size_t pixel_count);

// Color matrix transform (5x4) applied to all pixels (SIMD)
void apply_color_matrix_simd(uint8_t* pixels, size_t pixel_count, const double matrix[20]);

// Composite operations (SIMD)
void composite_over_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count);
void composite_add_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count);
void composite_multiply_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count);
void composite_screen_simd(uint8_t* dest, const uint8_t* src, size_t pixel_count);

// Brightness, contrast, saturation adjustments (SIMD)
void adjust_brightness_simd(uint8_t* pixels, size_t pixel_count, float brightness);
void adjust_contrast_simd(uint8_t* pixels, size_t pixel_count, float contrast);
void adjust_saturation_simd(uint8_t* pixels, size_t pixel_count, float saturation);
void adjust_opacity_simd(uint8_t* pixels, size_t pixel_count, float opacity);

// Invert colors (SIMD)
void invert_colors_simd(uint8_t* pixels, size_t pixel_count);

// Gaussian blur separable pass (SIMD)
void blur_horizontal_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, float radius);
void blur_vertical_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, float radius);

// Fast box blur (SIMD)
void box_blur_horizontal_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, int radius);
void box_blur_vertical_simd(uint8_t* dest, const uint8_t* src, int width, int height, int stride, int radius);

// Threshold / mask operations (SIMD)
void apply_alpha_mask_simd(uint8_t* pixels, size_t pixel_count, const uint8_t* mask);
void apply_luminance_to_alpha_simd(uint8_t* pixels, size_t pixel_count);

// ---- Existing Layout Operations -------------------------------------------

void layout_flex_sizing_simd(const float* grow_values,
                              const float* shrink_values,
                              const float* basis_values,
                              float* result_main_sizes,
                              size_t item_count,
                              float available_space,
                              bool is_row);

void layout_grid_track_sizing_simd(const float* min_sizes,
                                   const float* max_sizes,
                                   float* track_sizes,
                                   size_t track_count,
                                   float available_space);

void image_blur_row_simd(uint8_t* dest,
                         const uint8_t* src,
                         int width,
                         int height,
                         int bytes_per_pixel,
                         float blur_radius);

void image_blur_col_simd(uint8_t* dest,
                         const uint8_t* src,
                         int width,
                         int height,
                         int bytes_per_pixel,
                         float blur_radius);

void matrix4x4_transform_point(const float* matrix,
                              const float* points_in,
                              float* points_out,
                              size_t point_count);

// ---- 8-bit SIMD helpers for pixel manipulation ---------------------------

uint8x16 load_uint8x16(const uint8_t* ptr);
void store_uint8x16(uint8_t* ptr, const uint8x16& v);

uint8x16 blend_alpha_uint8x16(const uint8x16& dest, const uint8x16& src);

} // namespace hre::simd
