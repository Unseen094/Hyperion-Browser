#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <utility>

namespace hre::render {

struct canvas_gradient {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float r0 = 0, r1 = 0;
    bool is_radial = false;
    std::vector<std::pair<float, uint32_t>> color_stops;
};

struct canvas_state {
    float m[6] = {1, 0, 0, 1, 0, 0};
    float global_alpha = 1.0f;
    uint32_t fill_color = 0xFF000000;
    uint32_t stroke_color = 0xFF000000;
    float line_width = 1.0f;
    int line_cap = 0;
    int line_join = 0;
    float miter_limit = 10.0f;
    std::shared_ptr<canvas_gradient> fill_gradient;
    std::shared_ptr<canvas_gradient> stroke_gradient;
    float shadow_offset_x = 0, shadow_offset_y = 0;
    float shadow_blur = 0;
    uint32_t shadow_color = 0xFF000000;
    std::wstring font_family = L"sans-serif";
    float font_size = 16.0f;
    std::vector<std::pair<float, float>> dash;
};

class canvas_2d {
public:
    canvas_2d(int w, int h);

    void save();
    void restore();

    void scale(float x, float y);
    void rotate(float a);
    void translate(float x, float y);

    void set_fill_style(uint32_t c);
    void set_stroke_style(uint32_t c);
    void set_fill_gradient(float x0, float y0, float x1, float y1,
                           const std::vector<std::pair<float, uint32_t>>& stops);
    void set_stroke_gradient(float x0, float y0, float x1, float y1,
                             const std::vector<std::pair<float, uint32_t>>& stops);
    void set_global_alpha(float a);

    void fill_rect(float x, float y, float w, float h);
    void stroke_rect(float x, float y, float w, float h);
    void clear_rect(float x, float y, float w, float h);

    void begin_path();
    void move_to(float x, float y);
    void line_to(float x, float y);
    void arc(float x, float y, float r, float sa, float ea, bool ccw = false);
    void close_path();
    void fill();
    void stroke();

    void fill_text(const std::wstring& text, float x, float y);
    void draw_image(const std::vector<uint32_t>& img, int sw, int sh,
                    float dx, float dy, float dw, float dh);

    const uint32_t* pixels() const { return pixels_.data(); }
    std::vector<uint32_t> get_image_data(int x, int y, int w, int h) const;

    canvas_gradient create_linear_gradient(float x0, float y0, float x1, float y1);
    canvas_gradient create_radial_gradient(float x0, float y0, float r0,
                                           float x1, float y1, float r1);
    void add_color_stop(canvas_gradient& g, float pos, uint32_t color);

private:
    void fill_path(const std::vector<std::pair<float, float>>& path);
    void stroke_path(const std::vector<std::pair<float, float>>& path);
    void apply_shadow(const std::vector<std::pair<float, float>>& path);

    void apply_transform(float& x, float& y) const;
    void set_pixel(int x, int y, uint32_t color);
    uint32_t blend(uint32_t src, uint32_t dst, float alpha) const;

    std::vector<canvas_state> state_stack_;
    std::vector<std::pair<float, float>> current_path_;
    int width_ = 0;
    int height_ = 0;
    std::vector<uint32_t> pixels_;

    canvas_state& state() { return state_stack_.back(); }
    const canvas_state& state() const { return state_stack_.back(); }
};

} // namespace hre::render
