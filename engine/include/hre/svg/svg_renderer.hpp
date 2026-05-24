#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace hre::svg {

enum class svg_element_type {
    UNKNOWN,
    SVG,
    RECT,
    CIRCLE,
    ELLIPSE,
    LINE,
    POLYLINE,
    POLYGON,
    PATH,
    TEXT,
    G,
    DEFS,
    USE,
    LINEAR_GRADIENT,
    RADIAL_GRADIENT,
    CLIP_PATH,
    MASK
};

struct svg_transform {
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f;
    float e = 0.0f, f = 0.0f;

    void identity() { a = 1; b = 0; c = 0; d = 1; e = 0; f = 0; }
    void translate(float tx, float ty) { e += tx; f += ty; }
    void scale(float sx, float sy) { a *= sx; d *= sy; }
    void rotate(float angle, float cx = 0, float cy = 0);
};

struct svg_color {
    float r = 0, g = 0, b = 0, a = 1;

    static svg_color from_rgb(uint8_t ri, uint8_t gi, uint8_t bi) {
        svg_color c;
        c.r = ri / 255.0f; c.g = gi / 255.0f; c.b = bi / 255.0f;
        return c;
    }
};

struct svg_paint {
    enum class type { NONE, COLOR, GRADIENT, PATTERN } paint_type;

    svg_color color;
    std::wstring gradient_id;
    std::wstring pattern_id;
};

 struct svg_style {
 svg_paint fill;
 svg_paint stroke;

 svg_style() :
 fill({svg_paint::type::COLOR}),
 stroke({svg_paint::type::NONE}) {
 fill.color = svg_color{0, 0, 0, 1};
 }
    float stroke_width = 1.0f;
    float fill_opacity = 1.0f;
    float stroke_opacity = 1.0f;
    float opacity = 1.0f;
    svg_transform transform;
    std::wstring clip_path;
    std::wstring mask;
};

struct svg_point {
    float x = 0, y = 0;
};

struct svg_rect {
    float x = 0, y = 0, width = 0, height = 0, rx = 0, ry = 0;
};

struct svg_circle {
    float cx = 0, cy = 0, r = 0;
};

struct svg_path_command {
    enum class type { MOVE, LINE, HORIZ, VERT, CUBIC, QUAD, ARC, CLOSE } cmd;
    float args[7] = {0};
    int num_args = 0;
};

struct svg_gradient_stop {
    float offset = 0;
    svg_color color;
    float opacity = 1.0f;
};

struct svg_element {
    svg_element_type type = svg_element_type::UNKNOWN;
    std::map<std::wstring, std::wstring> attributes;
    std::vector<std::unique_ptr<svg_element>> children;
    svg_style style;
    std::wstring id;

    const svg_rect* as_rect() const;
    const svg_circle* as_circle() const;
};

class svg_renderer {
public:
    svg_renderer();
    ~svg_renderer();

    bool load_from_string(const std::wstring& svg_content);
    bool load_from_file(const std::wstring& filename);

    void render(void* context);

    const std::vector<std::unique_ptr<svg_element>>& definitions() const { return m_defs; }

private:
    std::unique_ptr<svg_element> m_root;
    std::vector<std::unique_ptr<svg_element>> m_defs;
    std::map<std::wstring, svg_element*> m_named_elements;
    void* m_render_context = nullptr;

    void render_element(const svg_element& elem, void* context);
    void apply_style(const svg_style& style, void* context);

    svg_element* get_element_by_id(const std::wstring& id);
};

} // namespace hre::svg