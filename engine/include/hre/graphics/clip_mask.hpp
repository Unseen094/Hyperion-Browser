#pragma once

#include <hre/graphics/matrix4x4.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace hre::graphics {

// ---- Basic Geometry Types -------------------------------------------------

struct point2 { float x, y; };
struct point3 { float x, y, z; };
struct size2  { float w, h; };
struct rect2  { float x, y, w, h; };

struct rounded_rect {
    float x, y, w, h;
    float rx, ry; // corner radii
    // Individual corner radii
    float rx_tl, ry_tl;
    float rx_tr, ry_tr;
    float rx_br, ry_br;
    float rx_bl, ry_bl;

    rounded_rect() : x(0), y(0), w(0), h(0), rx(0), ry(0),
                     rx_tl(0), ry_tl(0), rx_tr(0), ry_tr(0),
                     rx_br(0), ry_br(0), rx_bl(0), ry_bl(0) {}

    rounded_rect(float x, float y, float w, float h, float r)
        : x(x), y(y), w(w), h(h), rx(r), ry(r),
          rx_tl(r), ry_tl(r), rx_tr(r), ry_tr(r),
          rx_br(r), ry_br(r), rx_bl(r), ry_bl(r) {}

    void set_uniform_radius(float r) {
        rx = ry = rx_tl = ry_tl = rx_tr = ry_tr = rx_br = ry_br = rx_bl = ry_bl = r;
    }

    bool contains(float px, float py) const;
};

// ---- Clip Path ------------------------------------------------------------

enum class clip_path_type {
    NONE,
    INSET,
    CIRCLE,
    ELLIPSE,
    POLYGON,
    PATH,
    URL  // SVG clip-path reference
};

struct clip_path_inset {
    float top = 0, right = 0, bottom = 0, left = 0;
    float round_rx = 0, round_ry = 0;
};

struct clip_path_circle {
    float cx = 0.5f, cy = 0.5f;
    float r = 0.5f;
    bool at_center = true;
};

struct clip_path_ellipse {
    float cx = 0.5f, cy = 0.5f;
    float rx = 0.5f, ry = 0.5f;
};

struct clip_path_polygon {
    std::vector<std::pair<float, float>> points;
};

enum class path_command_type {
    MOVE_TO,
    LINE_TO,
    CURVE_TO,
    QUAD_TO,
    ARC_TO,
    CLOSE_PATH,
    HORIZONTAL_LINE_TO,
    VERTICAL_LINE_TO,
    SMOOTH_CURVE_TO,
    SMOOTH_QUAD_TO
};

struct path_command {
    path_command_type type;
    float args[7] = {0};
    int arg_count = 0;
    bool relative = false;
};

struct clip_path_path {
    std::vector<path_command> commands;
    std::wstring fill_rule; // "nonzero" or "evenodd"
};

struct clip_path {
    clip_path_type type = clip_path_type::NONE;
    clip_path_inset inset;
    clip_path_circle circle;
    clip_path_ellipse ellipse;
    clip_path_polygon polygon;
    clip_path_path path;
    std::wstring url;
    std::wstring fill_rule = L"nonzero"; // or "evenodd"

    static clip_path parse(const std::wstring& css_value);
    bool contains(float px, float py, float bounding_w, float bounding_h) const;
};

// ---- SVG clip-path resolver -----------------------------------------------

struct svg_clip_rule {
    std::vector<path_command> commands;
    std::wstring clip_rule; // "nonzero" or "evenodd"
};

class clip_path_resolver {
public:
    clip_path_resolver();

    static bool point_in_polygon(float px, float py, const std::vector<std::pair<float, float>>& polygon);
    static bool point_in_path(float px, float py, const std::vector<path_command>& commands, const std::wstring& fill_rule);
    static bool point_in_ellipse(float px, float py, float cx, float cy, float rx, float ry);

    // Evaluate a set of path commands and produce y-span for a given scanline
    static bool evaluate_path_at_y(const std::vector<path_command>& commands, float y,
                                   std::vector<float>& x_intersections, const std::wstring& fill_rule);

    // Rasterize a clip path to a binary mask (alpha channel)
    static bool rasterize_clip_path(const clip_path& path, float bounds_w, float bounds_h,
                                    uint8_t* mask, int mask_stride);

    // Rasterize from an SVG clip-path element
    static bool rasterize_svg_clip(const std::vector<path_command>& commands, float bounds_w, float bounds_h,
                                   uint8_t* mask, int mask_stride, const std::wstring& fill_rule);
};

// ---- Mask Image -----------------------------------------------------------

struct mask_image {
    enum mask_type { ALPHA, LUMINANCE };
    enum mask_mode { MATCH_SOURCE, NO_ZERO, NO_ONE, STRIDE };

    std::wstring url;
    mask_type type = ALPHA;
    mask_mode mode = MATCH_SOURCE;
    float x = 0, y = 0, width = 0, height = 0;
    std::wstring repeat = L"no-repeat";
    std::wstring position = L"center";
    std::wstring size = L"auto";
    std::vector<uint8_t> mask_data; // pre-decoded mask pixel data
    int mask_width = 0, mask_height = 0;

    static mask_image parse(const std::wstring& css_value);
    static std::vector<mask_image> parse_mask_image_list(const std::wstring& css_value);

    bool is_valid() const { return !mask_data.empty(); }
    void clear() { mask_data.clear(); mask_width = mask_height = 0; }
};

// ---- Mask Compositor ------------------------------------------------------

struct mask_apply_params {
    enum composite_mode { ADD, SUBTRACT, INTERSECT, EXCLUDE };

    composite_mode mode = ADD;
    float opacity = 1.0f;
    bool invert = false;
};

class mask_compositor {
public:
    mask_compositor();

    // Apply a mask image to pixel data
    static bool apply_mask(uint8_t* pixels, int width, int height, int stride,
                           const uint8_t* mask_pixels, int mask_w, int mask_h,
                           const mask_apply_params& params = {});

    // Apply luminance-to-alpha mask
    static bool apply_luminance_mask(uint8_t* pixels, int width, int height, int stride,
                                     const uint8_t* mask, int mask_w, int mask_h,
                                     float luminance_threshold = 0.5f);

    // Apply multiple masks in sequence
    static bool apply_masks(uint8_t* pixels, int width, int height, int stride,
                            const std::vector<mask_image>& masks);

    // Composite two RGBA buffers using CSS mask-composite mode
    static bool composite_masks(uint8_t* dest, const uint8_t* src,
                                int width, int height, int stride,
                                mask_apply_params::composite_mode mode);

    // Blend mask into alpha channel
    static bool blend_mask_into_alpha(uint8_t* pixels, int width, int height, int stride,
                                      const uint8_t* mask, int mask_w, int mask_h,
                                      float opacity = 1.0f);
};

// ---- Clipping stack for rendering pipeline --------------------------------

class clip_stack {
public:
    clip_stack();

    void push_rect(float x, float y, float w, float h);
    void push_rounded_rect(const rounded_rect& rr);
    void push_path(const clip_path& path, float bounds_w, float bounds_h);

    void pop();

    bool is_anything_visible() const { return !m_fully_clipped; }
    bool current_clip_contains(float px, float py) const;

    const std::vector<rounded_rect>& clip_rects() const { return m_clip_rects; }
    const std::vector<uint8_t>& clip_mask() const { return m_clip_mask; }
    int clip_mask_width() const { return m_mask_w; }
    int clip_mask_height() const { return m_mask_h; }

    void clear();

private:
    struct clip_entry {
        enum type { RECT, ROUNDED_RECT, PATH };
        type clip_type;
        rounded_rect rect;
        std::vector<uint8_t> mask;
        int mask_w = 0, mask_h = 0;
    };

    std::vector<clip_entry> m_clip_stack;
    bool m_fully_clipped = false;
    std::vector<rounded_rect> m_clip_rects;
    std::vector<uint8_t> m_clip_mask;
    int m_mask_w = 0, m_mask_h = 0;

    void update_combined_clip();
};

} // namespace hre::graphics
