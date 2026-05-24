#pragma once

#include <hre/image/image_decoder.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace hre::image {

// ---- AVIF Decoder ---------------------------------------------------------

class avif_decoder : public image_decoder {
public:
    avif_decoder();
    ~avif_decoder() override;

    bool can_decode(const std::string& magic_bytes) const override;
    image_format format() const override { return image_format::AVIF; }

    bool decode(const uint8_t* data, size_t size, image_decode_result& result) override;

    // AVIF-specific configuration
    void set_ignore_color_profile(bool ignore) { m_ignore_icc = ignore; }
    void set_prefer_yuv(bool yuv) { m_prefer_yuv = yuv; }
    int max_threads() const { return m_max_threads; }
    void set_max_threads(int t) { m_max_threads = t; }

private:
    bool decode_avif_container(const uint8_t* data, size_t size, image_decode_result& result);
    bool decode_av1_bitstream(const uint8_t* data, size_t size, int width, int height, std::vector<uint8_t>& rgb_out);

    bool m_ignore_icc = false;
    bool m_prefer_yuv = false;
    int m_max_threads = 4;
};

// ---- WebP2 Decoder --------------------------------------------------------

class webp2_decoder : public image_decoder {
public:
    webp2_decoder();
    ~webp2_decoder() override;

    bool can_decode(const std::string& magic_bytes) const override;
    image_format format() const override { return image_format::WEBP; }

    bool decode(const uint8_t* data, size_t size, image_decode_result& result) override;

    // WebP2-specific configuration
    void set_quality_target(int q) { m_quality_target = q; }

private:
    bool decode_webp2_container(const uint8_t* data, size_t size, image_decode_result& result);
    bool decode_vp8l_bitstream(const uint8_t* data, size_t size, int width, int height, std::vector<uint8_t>& rgb_out);

    int m_quality_target = 0;
};

// ---- Image Transform Utilities --------------------------------------------

struct image_transform_params {
    float scale_x = 1.0f, scale_y = 1.0f;
    float rotation_degrees = 0.0f;
    float translate_x = 0.0f, translate_y = 0.0f;
    bool flip_horizontal = false;
    bool flip_vertical = false;
    uint32_t background_color = 0x00000000; // ARGB
};

struct image_filter_params {
    float brightness = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float hue_rotation = 0.0f;
    float blur_radius = 0.0f;
    float opacity = 1.0f;
    bool grayscale = false;
    bool sepia = false;
    bool invert = false;
};

struct image_composite_params {
    enum mode { OVER, ADD, MULTIPLY, SCREEN, OVERLAY, DARKEN, LIGHTEN, DIFFERENCE, EXCLUSION, SOURCE_ATOP, SOURCE_IN, SOURCE_OUT, DESTINATION_ATOP, DESTINATION_IN, DESTINATION_OUT, LIGHTER, COPY };
    mode composite_mode = OVER;
    float opacity = 1.0f;

    static float blend_over(float dst, float src, float alpha);
    static float blend_add(float dst, float src, float alpha);
    static float blend_multiply(float dst, float src, float alpha);
    static float blend_screen(float dst, float src, float alpha);
    static float blend_overlay(float dst, float src, float alpha);
    static float blend_difference(float dst, float src, float alpha);
};

// ---- Image Processing Pipeline -------------------------------------------

class image_processor {
public:
    static bool resize(const uint8_t* input, int in_w, int in_h, int channels,
                       uint8_t* output, int out_w, int out_h);

    static bool apply_transform(image_decode_result& img, const image_transform_params& params);
    static bool apply_filters(image_decode_result& img, const image_filter_params& params);
    static bool composite(image_decode_result& dst, const image_decode_result& src, int x, int y, const image_composite_params& params);

    static bool flip_horizontal(image_decode_result& img);
    static bool flip_vertical(image_decode_result& img);
    static bool rotate(image_decode_result& img, float degrees);

    static bool gaussian_blur(image_decode_result& img, float radius);
    static bool box_blur(image_decode_result& img, int radius);
    static bool unsharp_mask(image_decode_result& img, float amount, float radius);

    static bool convert_grayscale(image_decode_result& img);
    static bool convert_sepia(image_decode_result& img);
    static bool adjust_brightness_contrast(image_decode_result& img, float brightness, float contrast);
    static bool adjust_hue_saturation(image_decode_result& img, float hue, float saturation);

    // Color space conversion
    static bool srgb_to_linear(uint8_t* pixels, size_t count);
    static bool linear_to_srgb(uint8_t* pixels, size_t count);

private:
    static void convolution_2d(const uint8_t* input, uint8_t* output, int width, int height, int channels,
                               const float* kernel, int kernel_size);
    static void bilinear_interpolate(const uint8_t* src, int src_w, int src_h, int channels,
                                     uint8_t* dst, int dst_w, int dst_h);
};

} // namespace hre::image
