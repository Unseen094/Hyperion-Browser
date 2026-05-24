#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <map>

namespace hre::css {

enum class filter_function_type {
    BLUR,
    BRIGHTNESS,
    CONTRAST,
    DROP_SHADOW,
    GRAYSCALE,
    HUE_ROTATE,
    INVERT,
    OPACITY,
    SATURATE,
    SEPIA,
    URL,
    NONE
};

struct css_filter_function {
    filter_function_type type = filter_function_type::NONE;
    double amount = 0.0;         // for scalar filters (brightness, contrast, etc.)
    double amount2 = 0.0;        // secondary parameter (hue-rotate degrees)
    std::wstring url;            // for url() references to SVG filters

    // Drop-shadow specific
    double shadow_offset_x = 0.0;
    double shadow_offset_y = 0.0;
    double shadow_blur = 0.0;
    uint32_t shadow_color = 0xFF000000; // ARGB
};

struct css_filter_list {
    std::vector<css_filter_function> filters;
    bool is_empty() const { return filters.empty(); }
    void clear() { filters.clear(); }
    void add(const css_filter_function& f) { filters.push_back(f); }
};

// ---- SVG Filter Primitives -----------------------------------------------

enum class svg_filter_primitive_type {
    FE_GAUSSIAN_BLUR,
    FE_COLOR_MATRIX,
    FE_COMPONENT_TRANSFER,
    FE_COMPOSITE,
    FE_CONVOLVE_MATRIX,
    FE_DISPLACEMENT_MAP,
    FE_DROP_SHADOW,
    FE_FLOOD,
    FE_IMAGE,
    FE_MERGE,
    FE_MERGE_NODE,
    FE_MORPHOLOGY,
    FE_OFFSET,
    FE_TILE,
    FE_TURBULENCE,
    FE_BLEND,
    FE_DILATE,
    FE_ERODE
};

struct svg_filter_primitive {
    svg_filter_primitive_type type;
    std::wstring id;
    std::wstring input;  // SourceGraphic, SourceAlpha, BackgroundImage, BackgroundAlpha, FilterPrimitive, <previous-result-id>
    std::wstring result;

    std::vector<double> parameters;
    std::wstring in2; // second input for composite/blend

    // Color matrix values (20 values for 5x4 matrix)
    std::vector<double> color_matrix_values;

    // Morphology operator: "dilate" or "erode"
    std::wstring morphology_operator;
    double morphology_radius_x = 0;
    double morphology_radius_y = 0;

    // Turbulence
    std::wstring turbulence_type; // "fractalNoise" or "turbulence"
    double base_frequency_x = 0;
    double base_frequency_y = 0;
    int num_octaves = 1;
    uint32_t seed = 0;

    // Offset
    double dx = 0, dy = 0;
};

struct svg_filter {
    std::wstring id;
    double x = -0.1, y = -0.1, width = 1.2, height = 1.2;
    std::vector<svg_filter_primitive> primitives;
    std::wstring filter_units; // "objectBoundingBox" or "userSpaceOnUse"
    std::wstring primitive_units;
};

// ---- CSS Filter Engine ----------------------------------------------------

class filter_engine {
public:
    filter_engine();
    ~filter_engine();

    static css_filter_list parse_filters(const std::wstring& filter_string);
    static css_filter_list parse_backdrop_filter(const std::wstring& filter_string);

    // Apply a filter list to a pixel buffer (RGBA)
    bool apply_filters(const css_filter_list& filters,
                       uint8_t* pixels, int width, int height, int stride);

    // GAussian Blur
    static bool gaussian_blur(uint8_t* pixels, int width, int height, int stride, double radius);
    static bool box_blur(uint8_t* pixels, int width, int height, int stride, int radius);
    static bool fast_blur(uint8_t* pixels, int width, int height, int stride, double radius);

    // Color matrix filter (applies a 5x4 color matrix)
    static bool apply_color_matrix(uint8_t* pixels, int width, int height, int stride,
                                   const double matrix[20]);

    // Standard filter functions
    static bool brightness(uint8_t* pixels, int width, int height, int stride, double amount);
    static bool contrast(uint8_t* pixels, int width, int height, int stride, double amount);
    static bool grayscale(uint8_t* pixels, int width, int height, int stride, double amount);
    static bool sepia(uint8_t* pixels, int width, int height, int stride, double amount);
    static bool invert(uint8_t* pixels, int width, int height, int stride, double amount);
    static bool saturate(uint8_t* pixels, int width, int height, int stride, double amount);
    static bool hue_rotate(uint8_t* pixels, int width, int height, int stride, double degrees);
    static bool opacity(uint8_t* pixels, int width, int height, int stride, double amount);
    static bool drop_shadow(uint8_t* pixels, int width, int height, int stride,
                            double offset_x, double offset_y, double blur, uint32_t color);

    // SVG filter rendering
    bool apply_svg_filter(const svg_filter& filter,
                          uint8_t* source, uint8_t* background,
                          int width, int height, int stride,
                          uint8_t* output);

private:
    // Internal state for SVG filter chain
    struct filter_context {
        int width, height, stride;
        std::vector<uint8_t> source;
        std::vector<uint8_t> background;
        std::map<std::wstring, std::vector<uint8_t>> primitive_results;
    };

    bool apply_primitive(filter_context& ctx, const svg_filter_primitive& prim);
    bool apply_gaussian_blur_primitive(filter_context& ctx, const svg_filter_primitive& prim);
    bool apply_color_matrix_primitive(filter_context& ctx, const svg_filter_primitive& prim);
    bool apply_offset_primitive(filter_context& ctx, const svg_filter_primitive& prim);
    bool apply_flood_primitive(filter_context& ctx, const svg_filter_primitive& prim);
    bool apply_merge_primitive(filter_context& ctx, const svg_filter_primitive& prim);
    bool apply_turbulence_primitive(filter_context& ctx, const svg_filter_primitive& prim);
    bool apply_composite_primitive(filter_context& ctx, const svg_filter_primitive& prim);

    std::vector<uint8_t>* get_input_buffer(filter_context& ctx, const std::wstring& input_name);
};

// ---- Pre-computed Filter Matrices ----------------------------------------

namespace filter_matrices {
    constexpr double GRAYSCALE_MATRIX[20] = {
        0.2126, 0.7152, 0.0722, 0, 0,
        0.2126, 0.7152, 0.0722, 0, 0,
        0.2126, 0.7152, 0.0722, 0, 0,
        0,      0,      0,      1, 0
    };

    constexpr double SEPIA_MATRIX[20] = {
        0.393, 0.769, 0.189, 0, 0,
        0.349, 0.686, 0.168, 0, 0,
        0.272, 0.534, 0.131, 0, 0,
        0,     0,     0,     1, 0
    };

    constexpr double INVERT_MATRIX[20] = {
        -1,  0,  0, 0, 1,
         0, -1,  0, 0, 1,
         0,  0, -1, 0, 1,
         0,  0,  0, 1, 0
    };
}

} // namespace hre::css
