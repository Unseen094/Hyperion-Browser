#include <hre/render/canvas_2d.hpp>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <unordered_map>

namespace hre::render {

canvas_2d::canvas_2d(int w, int h)
    : width_(w), height_(h), pixels_(static_cast<size_t>(w) * h, 0xFFFFFFFF) {
    state_stack_.emplace_back();
}

void canvas_2d::save() {
    state_stack_.push_back(state_stack_.back());
    if (state_stack_.back().fill_gradient) {
        state_stack_.back().fill_gradient =
            std::make_shared<canvas_gradient>(*state_stack_.back().fill_gradient);
    }
    if (state_stack_.back().stroke_gradient) {
        state_stack_.back().stroke_gradient =
            std::make_shared<canvas_gradient>(*state_stack_.back().stroke_gradient);
    }
}

void canvas_2d::restore() {
    if (state_stack_.size() > 1) state_stack_.pop_back();
}

void canvas_2d::scale(float sx, float sy) {
    auto& s = state();
    float m[6] = {s.m[0]*sx, s.m[1]*sx, s.m[2]*sy, s.m[3]*sy, s.m[4], s.m[5]};
    std::copy(m, m + 6, s.m);
}

void canvas_2d::rotate(float a) {
    float c = std::cos(a), c_ = std::cos(-a);
    float si = std::sin(a);
    auto& s = state();
    float m[6] = {
        s.m[0]*c + s.m[2]*si, s.m[1]*c + s.m[3]*si,
        s.m[2]*c_ - s.m[0]*si, s.m[3]*c_ - s.m[1]*si,
        s.m[4], s.m[5]
    };
    std::copy(m, m + 6, s.m);
}

void canvas_2d::translate(float dx, float dy) {
    auto& s = state();
    s.m[4] += s.m[0]*dx + s.m[2]*dy;
    s.m[5] += s.m[1]*dx + s.m[3]*dy;
}

void canvas_2d::set_fill_style(uint32_t c) { state().fill_color = c; state().fill_gradient.reset(); }
void canvas_2d::set_stroke_style(uint32_t c) { state().stroke_color = c; state().stroke_gradient.reset(); }
void canvas_2d::set_global_alpha(float a) { state().global_alpha = a; }

void canvas_2d::set_fill_gradient(float x0, float y0, float x1, float y1,
                                   const std::vector<std::pair<float, uint32_t>>& stops) {
    auto g = std::make_shared<canvas_gradient>();
    g->x0 = x0; g->y0 = y0; g->x1 = x1; g->y1 = y1;
    g->is_radial = false;
    g->color_stops = stops;
    state().fill_gradient = g;
}

void canvas_2d::set_stroke_gradient(float x0, float y0, float x1, float y1,
                                     const std::vector<std::pair<float, uint32_t>>& stops) {
    auto g = std::make_shared<canvas_gradient>();
    g->x0 = x0; g->y0 = y0; g->x1 = x1; g->y1 = y1;
    g->is_radial = false;
    g->color_stops = stops;
    state().stroke_gradient = g;
}

void canvas_2d::apply_transform(float& x, float& y) const {
    const auto& s = state();
    float tx = x * s.m[0] + y * s.m[2] + s.m[4];
    float ty = x * s.m[1] + y * s.m[3] + s.m[5];
    x = tx; y = ty;
}

uint32_t canvas_2d::blend(uint32_t src, uint32_t dst, float alpha) const {
    uint32_t sa = static_cast<uint32_t>(((src >> 24) & 0xFF) * alpha);
    if (sa == 0) return dst;
    if (sa >= 255) return src;
    uint32_t da = (dst >> 24) & 0xFF;
    uint32_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint32_t a = sa + da * (255 - sa) / 255;
    if (a == 0) return 0;
    uint32_t r = (sr * sa + dr * da * (255 - sa) / 255) / a;
    uint32_t g = (sg * sa + dg * da * (255 - sa) / 255) / a;
    uint32_t b = (sb * sa + db * da * (255 - sa) / 255) / a;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void canvas_2d::set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    pixels_[static_cast<size_t>(y) * width_ + x] = blend(color, pixels_[static_cast<size_t>(y) * width_ + x], state().global_alpha);
}

void canvas_2d::fill_rect(float x, float y, float w, float h) {
    std::vector<std::pair<float, float>> path;
    path.emplace_back(x, y);
    path.emplace_back(x + w, y);
    path.emplace_back(x + w, y + h);
    path.emplace_back(x, y + h);
    path.emplace_back(x, y);
    fill_path(path);
}

void canvas_2d::stroke_rect(float x, float y, float w, float h) {
    std::vector<std::pair<float, float>> path;
    path.emplace_back(x, y);
    path.emplace_back(x + w, y);
    path.emplace_back(x + w, y + h);
    path.emplace_back(x, y + h);
    path.emplace_back(x, y);
    stroke_path(path);
}

void canvas_2d::clear_rect(float x, float y, float w, float h) {
    int ix = std::max(0, static_cast<int>(x));
    int iy = std::max(0, static_cast<int>(y));
    int iw = std::min(width_ - ix, static_cast<int>(w));
    int ih = std::min(height_ - iy, static_cast<int>(h));
    for (int row = iy; row < iy + ih; ++row) {
        std::fill(pixels_.begin() + static_cast<size_t>(row) * width_ + ix,
                  pixels_.begin() + static_cast<size_t>(row) * width_ + ix + iw,
                  0xFFFFFFFF);
    }
}

void canvas_2d::begin_path() { current_path_.clear(); }
void canvas_2d::move_to(float x, float y) { current_path_.emplace_back(x, y); }
void canvas_2d::line_to(float x, float y) { current_path_.emplace_back(x, y); }

void canvas_2d::arc(float cx, float cy, float r, float sa, float ea, bool ccw) {
    if (r < 0) return;
    int segments = std::max(6, static_cast<int>(std::abs(ea - sa) * r * 0.1f));
    if (segments > 360) segments = 360;
    float da = (ea - sa) / segments;
    if (ccw) da = -std::abs(da);
    else da = std::abs(da);
    float sign = (ea > sa) ? 1.0f : -1.0f;
    if (!ccw && ea < sa) da = std::abs(da);
    if (ccw && ea > sa) da = -std::abs(da);
    for (int i = 0; i <= segments; ++i) {
        float a = sa + (ea - sa) * i / segments;
        current_path_.emplace_back(cx + r * std::cos(a), cy + r * std::sin(a));
    }
}

void canvas_2d::close_path() {
    if (!current_path_.empty()) {
        current_path_.push_back(current_path_.front());
    }
}

void canvas_2d::fill_path(const std::vector<std::pair<float, float>>& path) {
    if (path.size() < 3) return;
    apply_shadow(path);
    auto& s = state();
    uint32_t color = s.fill_color;
    if (s.fill_gradient) {
        auto& g = *s.fill_gradient;
        float dx = g.x1 - g.x0, dy = g.y1 - g.y0;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;
    }

    int min_x = width_, min_y = height_, max_x = 0, max_y = 0;
    for (auto& pt : path) {
        float fx = pt.first, fy = pt.second;
        apply_transform(fx, fy);
        int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
        min_x = std::min(min_x, ix); min_y = std::min(min_y, iy);
        max_x = std::max(max_x, ix); max_y = std::max(max_y, iy);
    }
    min_x = std::max(0, min_x); min_y = std::max(0, min_y);
    max_x = std::min(width_ - 1, max_x); max_y = std::min(height_ - 1, max_y);

    for (int y = min_y; y <= max_y; ++y) {
        std::vector<int> xs;
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            float x1_ = path[i].first, y1_ = path[i].second;
            float x2_ = path[i + 1].first, y2_ = path[i + 1].second;
            apply_transform(x1_, y1_); apply_transform(x2_, y2_);
            if ((y1_ <= y && y2_ > y) || (y2_ <= y && y1_ > y)) {
                float t = (y - y1_) / (y2_ - y1_);
                xs.push_back(static_cast<int>(x1_ + t * (x2_ - x1_)));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            int x0 = std::max(min_x, xs[i]);
            int x1 = std::min(max_x, xs[i + 1]);
            for (int x = x0; x <= x1; ++x) set_pixel(x, y, color);
        }
    }
}

void canvas_2d::stroke_path(const std::vector<std::pair<float, float>>& path) {
    if (path.size() < 2) return;
    apply_shadow(path);
    uint32_t color = state().stroke_color;
    float w2 = state().line_width / 2.0f;

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        float x1 = path[i].first, y1 = path[i].second;
        float x2 = path[i + 1].first, y2 = path[i + 1].second;
        apply_transform(x1, y1); apply_transform(x2, y2);

        float dx = x2 - x1, dy = y2 - y1;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) continue;
        float nx = -dy / len * w2, ny = dx / len * w2;

        float px1 = x1 + nx, py1 = y1 + ny;
        float px2 = x2 + nx, py2 = y2 + ny;
        float qx1 = x1 - nx, qy1 = y1 - ny;
        float qx2 = x2 - nx, qy2 = y2 - ny;

        std::vector<std::pair<float, float>> seg;
        seg.emplace_back(px1, py1);
        seg.emplace_back(px2, py2);
        seg.emplace_back(qx2, qy2);
        seg.emplace_back(qx1, qy1);
        seg.emplace_back(px1, py1);
        fill_path(seg);
    }
}

void canvas_2d::apply_shadow(const std::vector<std::pair<float, float>>& path) {
    auto& s = state();
    if (s.shadow_blur < 0.001f && s.shadow_offset_x == 0 && s.shadow_offset_y == 0) return;
    std::vector<std::pair<float, float>> shadow_path = path;
    for (auto& pt : shadow_path) {
        pt.first += s.shadow_offset_x;
        pt.second += s.shadow_offset_y;
    }
    uint32_t save_color = s.fill_color;
    float save_alpha = s.global_alpha;
    s.fill_color = s.shadow_color;
    s.global_alpha = 0.5f;
    fill_path(shadow_path);
    s.fill_color = save_color;
    s.global_alpha = save_alpha;
}

void canvas_2d::fill() { fill_path(current_path_); }
void canvas_2d::stroke() { stroke_path(current_path_); }

void canvas_2d::fill_text(const std::wstring& text, float x, float y) {
    (void)text;
    uint32_t save = state().fill_color;
    float char_w = state().font_size * 0.6f;
    for (size_t i = 0; i < text.size(); ++i) {
        fill_rect(x + i * char_w, y - state().font_size, char_w * 0.8f, state().font_size);
    }
}

void canvas_2d::draw_image(const std::vector<uint32_t>& img, int sw, int sh,
                            float dx, float dy, float dw, float dh) {
    for (int row = 0; row < static_cast<int>(dh) && row < sh; ++row) {
        for (int col = 0; col < static_cast<int>(dw) && col < sw; ++col) {
            int sx = col * sw / static_cast<int>(dw);
            int sy = row * sh / static_cast<int>(dh);
            uint32_t c = img[static_cast<size_t>(sy) * sw + sx];
            set_pixel(static_cast<int>(dx) + col, static_cast<int>(dy) + row, c);
        }
    }
}

std::vector<uint32_t> canvas_2d::get_image_data(int x, int y, int w, int h) const {
    std::vector<uint32_t> result(static_cast<size_t>(w) * h);
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int sx = x + col, sy = y + row;
            if (sx >= 0 && sx < width_ && sy >= 0 && sy < height_) {
                result[static_cast<size_t>(row) * w + col] = pixels_[static_cast<size_t>(sy) * width_ + sx];
            }
        }
    }
    return result;
}

canvas_gradient canvas_2d::create_linear_gradient(float x0, float y0, float x1, float y1) {
    canvas_gradient g;
    g.x0 = x0; g.y0 = y0; g.x1 = x1; g.y1 = y1;
    g.is_radial = false;
    return g;
}

canvas_gradient canvas_2d::create_radial_gradient(float x0, float y0, float r0,
                                                    float x1, float y1, float r1) {
    canvas_gradient g;
    g.x0 = x0; g.y0 = y0; g.r0 = r0;
    g.x1 = x1; g.y1 = y1; g.r1 = r1;
    g.is_radial = true;
    return g;
}

void canvas_2d::add_color_stop(canvas_gradient& g, float pos, uint32_t color) {
    g.color_stops.emplace_back(pos, color);
    std::sort(g.color_stops.begin(), g.color_stops.end(),
              [](auto& a, auto& b) { return a.first < b.first; });
}

} // namespace hre::render
