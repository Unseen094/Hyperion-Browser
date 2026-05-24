#include <hre/graphics/clip_mask.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stack>
#include <sstream>

namespace hre::graphics {

// ---- rounded_rect ---------------------------------------------------------

bool rounded_rect::contains(float px, float py) const {
    if (px < x || px > x + w || py < y || py > y + h) return false;

    auto corner_test = [&](float cx, float cy, float rx, float ry) -> bool {
        if (rx <= 0 || ry <= 0) return true;
        float dx = (px - cx) / rx;
        float dy = (py - cy) / ry;
        return (dx * dx + dy * dy) <= 1.0f;
    };

    if (px < x + rx_tl && py < y + ry_tl) return corner_test(x + rx_tl, y + ry_tl, rx_tl, ry_tl);
    if (px > x + w - rx_tr && py < y + ry_tr) return corner_test(x + w - rx_tr, y + ry_tr, rx_tr, ry_tr);
    if (px < x + rx_bl && py > y + h - ry_bl) return corner_test(x + rx_bl, y + h - ry_bl, rx_bl, ry_bl);
    if (px > x + w - rx_br && py > y + h - ry_br) return corner_test(x + w - rx_br, y + h - ry_br, rx_br, ry_br);

    return true;
}

// ---- clip_path ------------------------------------------------------------

clip_path clip_path::parse(const std::wstring& css_value) {
    clip_path result;
    if (css_value.empty() || css_value == L"none") return result;

    if (css_value.find(L"inset(") == 0) {
        result.type = clip_path_type::INSET;
        std::wistringstream ss(css_value.substr(6, css_value.find(L')') - 6));
        wchar_t comma;
        ss >> result.inset.top >> comma >> result.inset.right >> comma
           >> result.inset.bottom >> comma >> result.inset.left;
        auto round_pos = css_value.find(L"round");
        if (round_pos != std::wstring::npos) {
            std::wistringstream rs(css_value.substr(round_pos + 5));
            rs >> result.inset.round_rx >> comma >> result.inset.round_ry;
        }
    } else if (css_value.find(L"circle(") == 0) {
        result.type = clip_path_type::CIRCLE;
    } else if (css_value.find(L"ellipse(") == 0) {
        result.type = clip_path_type::ELLIPSE;
    } else if (css_value.find(L"polygon(") == 0) {
        result.type = clip_path_type::POLYGON;
    } else if (css_value.find(L"url(") == 0) {
        result.type = clip_path_type::URL;
        size_t start = css_value.find(L'(') + 1;
        size_t end = css_value.find(L')');
        if (start < end) result.url = css_value.substr(start, end - start);
    } else {
        result.type = clip_path_type::PATH;
    }

    return result;
}

bool clip_path::contains(float px, float py, float bounding_w, float bounding_h) const {
    switch (type) {
        case clip_path_type::NONE:
            return true;
        case clip_path_type::INSET:
            return px >= inset.left && px <= bounding_w - inset.right &&
                   py >= inset.top && py <= bounding_h - inset.bottom;
        case clip_path_type::CIRCLE: {
            float cx = circle.at_center ? bounding_w / 2 : circle.cx * bounding_w;
            float cy = circle.at_center ? bounding_h / 2 : circle.cy * bounding_h;
            float r = circle.r * std::max(bounding_w, bounding_h);
            float dx = px - cx, dy = py - cy;
            return (dx * dx + dy * dy) <= r * r;
        }
        case clip_path_type::ELLIPSE: {
            float cx = ellipse.cx * bounding_w;
            float cy = ellipse.cy * bounding_h;
            float rx = ellipse.rx * bounding_w;
            float ry = ellipse.ry * bounding_h;
            return clip_path_resolver::point_in_ellipse(px, py, cx, cy, rx, ry);
        }
        case clip_path_type::POLYGON:
            return clip_path_resolver::point_in_polygon(px, py, polygon.points);
        case clip_path_type::PATH:
            return clip_path_resolver::point_in_path(px, py, path.commands, path.fill_rule);
        default:
            return true;
    }
}

// ---- clip_path_resolver ---------------------------------------------------

clip_path_resolver::clip_path_resolver() = default;

bool clip_path_resolver::point_in_polygon(float px, float py, const std::vector<std::pair<float, float>>& polygon) {
    bool inside = false;
    size_t n = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if ((polygon[i].second > py) != (polygon[j].second > py) &&
            px < (polygon[j].first - polygon[i].first) * (py - polygon[i].second) / (polygon[j].second - polygon[i].second) + polygon[i].first) {
            inside = !inside;
        }
    }
    return inside;
}

bool clip_path_resolver::point_in_path(float px, float py, const std::vector<path_command>& commands, const std::wstring& fill_rule) {
    std::vector<float> intersections;
    if (evaluate_path_at_y(commands, py, intersections, fill_rule)) {
        size_t count = 0;
        for (float ix : intersections) {
            if (ix <= px) ++count;
        }
        if (fill_rule == L"evenodd") return count % 2 == 1;
        return count > 0;
    }
    return false;
}

bool clip_path_resolver::point_in_ellipse(float px, float py, float cx, float cy, float rx, float ry) {
    if (rx <= 0 || ry <= 0) return false;
    float dx = (px - cx) / rx;
    float dy = (py - cy) / ry;
    return (dx * dx + dy * dy) <= 1.0f;
}

bool clip_path_resolver::evaluate_path_at_y(const std::vector<path_command>& commands, float y,
                                             std::vector<float>& x_intersections, const std::wstring& fill_rule) {
    (void)fill_rule;
    float cur_x = 0, cur_y = 0;
    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case path_command_type::MOVE_TO:
                cur_x = cmd.args[0]; cur_y = cmd.args[1]; break;
            case path_command_type::LINE_TO: {
                float x0 = cur_x, y0 = cur_y;
                float x1 = cmd.args[0], y1 = cmd.args[1];
                if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                    float t = (y - y0) / (y1 - y0);
                    x_intersections.push_back(x0 + t * (x1 - x0));
                }
                cur_x = x1; cur_y = y1; break;
            }
            case path_command_type::CLOSE_PATH:
                break;
            default:
                break;
        }
    }
    return !x_intersections.empty();
}

bool clip_path_resolver::rasterize_clip_path(const clip_path& path, float bounds_w, float bounds_h,
                                              uint8_t* mask, int mask_stride) {
    int w = static_cast<int>(bounds_w);
    int h = static_cast<int>(bounds_h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool inside = path.contains(static_cast<float>(x), static_cast<float>(y), bounds_w, bounds_h);
            mask[y * mask_stride + x] = inside ? 255 : 0;
        }
    }
    return true;
}

bool clip_path_resolver::rasterize_svg_clip(const std::vector<path_command>& commands, float bounds_w, float bounds_h,
                                             uint8_t* mask, int mask_stride, const std::wstring& fill_rule) {
    int w = static_cast<int>(bounds_w);
    int h = static_cast<int>(bounds_h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool inside = point_in_path(static_cast<float>(x), static_cast<float>(y), commands, fill_rule);
            mask[y * mask_stride + x] = inside ? 255 : 0;
        }
    }
    return true;
}

// ---- mask_image -----------------------------------------------------------

mask_image mask_image::parse(const std::wstring& css_value) {
    mask_image mask;
    if (css_value.empty() || css_value == L"none") return mask;

    if (css_value.find(L"url(") == 0) {
        size_t start = css_value.find(L'(') + 1;
        size_t end = css_value.find(L')');
        if (start < end) mask.url = css_value.substr(start, end - start);
    }
    if (css_value.find(L"luminance") != std::wstring::npos) mask.type = LUMINANCE;

    return mask;
}

std::vector<mask_image> mask_image::parse_mask_image_list(const std::wstring& css_value) {
    std::vector<mask_image> masks;
    if (css_value.empty()) return masks;

    size_t pos = 0;
    while (pos < css_value.size()) {
        while (pos < css_value.size() && css_value[pos] == L' ') ++pos;
        if (pos >= css_value.size()) break;
        size_t comma = css_value.find(L',', pos);
        std::wstring segment = css_value.substr(pos, comma - pos);
        masks.push_back(parse(segment));
        pos = (comma == std::wstring::npos) ? css_value.size() : comma + 1;
    }

    return masks;
}

// ---- mask_compositor ------------------------------------------------------

mask_compositor::mask_compositor() = default;

bool mask_compositor::apply_mask(uint8_t* pixels, int width, int height, int stride,
                                  const uint8_t* mask_pixels, int mask_w, int mask_h,
                                  const mask_apply_params& params) {
    if (!pixels || !mask_pixels) return false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int mx = x % mask_w, my = y % mask_h;
            float mask_val = mask_pixels[my * mask_w + mx] / 255.0f;
            if (params.invert) mask_val = 1.0f - mask_val;
            mask_val *= params.opacity;

            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            pixels[idx + 3] = static_cast<uint8_t>(pixels[idx + 3] * mask_val);
        }
    }
    return true;
}

bool mask_compositor::apply_luminance_mask(uint8_t* pixels, int width, int height, int stride,
                                            const uint8_t* mask, int mask_w, int mask_h,
                                            float luminance_threshold) {
    (void)luminance_threshold;
    return apply_mask(pixels, width, height, stride, mask, mask_w, mask_h);
}

bool mask_compositor::apply_masks(uint8_t* pixels, int width, int height, int stride,
                                   const std::vector<mask_image>& masks) {
    for (const auto& m : masks) {
        if (m.is_valid()) {
            mask_apply_params params;
            apply_mask(pixels, width, height, stride, m.mask_data.data(), m.mask_width, m.mask_height, params);
        }
    }
    return true;
}

bool mask_compositor::composite_masks(uint8_t* dest, const uint8_t* src,
                                       int width, int height, int stride,
                                       mask_apply_params::composite_mode mode) {
    (void)mode;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * stride + static_cast<size_t>(x);
            dest[idx] = std::max(dest[idx], src[idx]);
        }
    }
    return true;
}

bool mask_compositor::blend_mask_into_alpha(uint8_t* pixels, int width, int height, int stride,
                                             const uint8_t* mask, int mask_w, int mask_h,
                                             float opacity) {
    mask_apply_params params;
    params.opacity = opacity;
    return apply_mask(pixels, width, height, stride, mask, mask_w, mask_h, params);
}

// ---- clip_stack -----------------------------------------------------------

clip_stack::clip_stack() = default;

void clip_stack::push_rect(float x, float y, float w, float h) {
    clip_entry entry;
    entry.clip_type = clip_entry::RECT;
    entry.rect = rounded_rect(x, y, w, h, 0);
    m_clip_stack.push_back(entry);
    update_combined_clip();
}

void clip_stack::push_rounded_rect(const rounded_rect& rr) {
    clip_entry entry;
    entry.clip_type = clip_entry::ROUNDED_RECT;
    entry.rect = rr;
    m_clip_stack.push_back(entry);
    update_combined_clip();
}

void clip_stack::push_path(const clip_path& path, float bounds_w, float bounds_h) {
    clip_entry entry;
    entry.clip_type = clip_entry::PATH;
    entry.mask_w = static_cast<int>(bounds_w);
    entry.mask_h = static_cast<int>(bounds_h);
    entry.mask.resize(static_cast<size_t>(entry.mask_w) * entry.mask_h, 255);
    clip_path_resolver::rasterize_clip_path(path, bounds_w, bounds_h, entry.mask.data(), entry.mask_w);
    m_clip_stack.push_back(entry);
    update_combined_clip();
}

void clip_stack::pop() {
    if (!m_clip_stack.empty()) {
        m_clip_stack.pop_back();
        update_combined_clip();
    }
}

bool clip_stack::current_clip_contains(float px, float py) const {
    for (const auto& entry : m_clip_stack) {
        switch (entry.clip_type) {
            case clip_entry::RECT:
            case clip_entry::ROUNDED_RECT:
                if (!entry.rect.contains(px, py)) return false;
                break;
            case clip_entry::PATH: {
                int ix = static_cast<int>(px);
                int iy = static_cast<int>(py);
                if (ix >= 0 && ix < entry.mask_w && iy >= 0 && iy < entry.mask_h) {
                    if (!entry.mask[iy * entry.mask_w + ix]) return false;
                }
                break;
            }
        }
    }
    return true;
}

void clip_stack::clear() {
    m_clip_stack.clear();
    m_fully_clipped = false;
    m_clip_rects.clear();
    m_clip_mask.clear();
    m_mask_w = m_mask_h = 0;
}

void clip_stack::update_combined_clip() {
    m_clip_rects.clear();
    for (const auto& entry : m_clip_stack) {
        if (entry.clip_type == clip_entry::RECT || entry.clip_type == clip_entry::ROUNDED_RECT) {
            m_clip_rects.push_back(entry.rect);
        }
    }

    if (!m_clip_stack.empty() && m_clip_stack.back().clip_type == clip_entry::PATH) {
        m_clip_mask = m_clip_stack.back().mask;
        m_mask_w = m_clip_stack.back().mask_w;
        m_mask_h = m_clip_stack.back().mask_h;
    }
}

} // namespace hre::graphics
