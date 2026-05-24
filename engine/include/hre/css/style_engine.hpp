#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <functional>
#include "hre/dom/node.hpp"

namespace hre::css {

// CSS Value Types
enum class CssUnit {
    None, Pixel, Percent, Em, Rem, Auto, Inherit, Initial, Unset, Revert,
    Vw, Vh, Vmin, Vmax, Ch, Ex, Cm, Mm, In, Pt, Pc, Deg, Rad, Grad, Turn, S, Ms, Hz, Khz, Dpi, Dpcm, Dppx, Fr
};

struct CssValue {
    double number = 0.0;
    CssUnit unit = CssUnit::None;
    std::wstring raw;
    std::wstring string_value; // for non-numeric values like colors, keywords

    static CssValue parse(const std::wstring& str);
    double to_pixels(double base_size = 16.0, double viewport_w = 0, double viewport_h = 0) const;
    double to_degrees() const;
    double to_seconds() const;
};

// CSS Color with wide gamut support
struct CssColor {
    std::variant<std::monostate, std::tuple<double,double,double,double>, std::wstring> value;
    std::wstring color_space = L"srgb"; // srgb, srgb-linear, display-p3, a98-rgb, prophoto-rgb, rec2020, xyz, xyz-d50, xyz-d65
    bool is_current_color = false;
    bool is_transparent = true;

    static CssColor parse(const std::wstring& str);
    std::tuple<double,double,double,double> to_rgba() const;
};

// Origin layer for cascade
enum class CascadeOrigin : int {
    UA = 0,
    User = 1,
    Author = 2,
    Animation = 3,
    Transition = 4,
    Override = 5
};

// CSS at-rule types
enum class AtRuleType {
    None, Media, Supports, FontFace, Keyframes, Import, Page, Namespace, Layer, Property, CounterStyle, Container, Scope
};

struct MediaQuery {
    std::wstring raw;
    bool negated = false;
    std::wstring media_type = L"all";
    std::vector<std::pair<std::wstring, std::wstring>> features; // name, value
    bool evaluate(float viewport_width, float viewport_height, float device_pixel_ratio) const;
};

struct SupportsCondition {
    std::wstring raw;
    bool negated = false;
    std::vector<std::wstring> declarations; // "property: value" strings
    bool evaluate() const;
};

struct FontFace {
    std::wstring font_family;
    std::wstring src;
    std::wstring font_style = L"normal";
    std::wstring font_weight = L"normal";
    std::wstring font_stretch = L"normal";
    std::wstring font_display = L"auto";
    std::map<std::wstring, std::wstring> descriptors;
};

struct Keyframe {
    std::vector<std::wstring> selectors; // "from", "to", "50%", etc.
    std::map<std::wstring, std::wstring> declarations;
};

struct KeyframesRule {
    std::wstring name;
    std::vector<Keyframe> keyframes;
};

struct ParsedRule {
    std::vector<std::wstring> selectors;
    std::vector<std::pair<std::wstring, std::wstring>> declarations; // property, value
    bool important = false;
    CascadeOrigin origin = CascadeOrigin::Author;
    AtRuleType at_rule_type = AtRuleType::None;
    std::shared_ptr<MediaQuery> media_query;
    std::shared_ptr<SupportsCondition> supports_condition;
    int source_order = 0;
};

// Computed Style Properties - extended
struct ComputedStyle {
    // Display & Position
    std::wstring display = L"inline";
    std::wstring position = L"static";
    bool is_replaced = false;
    bool is_inline = true;

    // Box Model
    CssValue width = {0, CssUnit::Auto, L"auto"};
    CssValue height = {0, CssUnit::Auto, L"auto"};
    CssValue min_width = {0, CssUnit::Auto, L"0"};
    CssValue max_width = {0, CssUnit::None, L"none"};
    CssValue min_height = {0, CssUnit::Auto, L"0"};
    CssValue max_height = {0, CssUnit::None, L"none"};
    std::wstring box_sizing = L"content-box";
    CssValue margin_top = {0, CssUnit::Pixel, L"0"};
    CssValue margin_right = {0, CssUnit::Pixel, L"0"};
    CssValue margin_bottom = {0, CssUnit::Pixel, L"0"};
    CssValue margin_left = {0, CssUnit::Pixel, L"0"};
    CssValue padding_top = {0, CssUnit::Pixel, L"0"};
    CssValue padding_right = {0, CssUnit::Pixel, L"0"};
    CssValue padding_bottom = {0, CssUnit::Pixel, L"0"};
    CssValue padding_left = {0, CssUnit::Pixel, L"0"};
    CssValue border_top_width = {0, CssUnit::Pixel, L"0"};
    CssValue border_right_width = {0, CssUnit::Pixel, L"0"};
    CssValue border_bottom_width = {0, CssUnit::Pixel, L"0"};
    CssValue border_left_width = {0, CssUnit::Pixel, L"0"};

    // Position offsets
    CssValue top = {0, CssUnit::Auto, L"auto"};
    CssValue right = {0, CssUnit::Auto, L"auto"};
    CssValue bottom = {0, CssUnit::Auto, L"auto"};
    CssValue left = {0, CssUnit::Auto, L"auto"};
    std::wstring inset = L"auto";
    int z_index = 0;
    bool has_z_index = false;

    // Overflow
    std::wstring overflow_x = L"visible";
    std::wstring overflow_y = L"visible";
    std::wstring overflow = L"visible";

    // Typography
    std::wstring font_family = L"Segoe UI";
    CssValue font_size = {16.0, CssUnit::Pixel, L"16px"};
    std::wstring font_weight = L"normal";
    std::wstring font_style = L"normal";
    std::wstring font_stretch = L"normal";
    CssValue line_height = {1.2, CssUnit::None, L"normal"};
    std::wstring font_variant = L"normal";
    CssValue letter_spacing = {0, CssUnit::Pixel, L"normal"};
    CssValue word_spacing = {0, CssUnit::Pixel, L"normal"};
    std::wstring white_space = L"normal";
    std::wstring word_break = L"normal";
    std::wstring overflow_wrap = L"normal";
    std::wstring text_align = L"left";
    std::wstring text_transform = L"none";
    std::wstring text_decoration_line = L"none";
    std::wstring text_decoration_color = L"currentColor";
    std::wstring text_decoration_style = L"solid";
    CssValue text_decoration_thickness = {0, CssUnit::Auto, L"auto"};
    std::wstring text_shadow = L"none";
    std::wstring direction = L"ltr";
    std::wstring unicode_bidi = L"normal";

    // Colors
    CssColor color = CssColor::parse(L"#000000");
    CssColor background_color;
    std::wstring background_image = L"none";
    std::wstring background_gradient = L"";
    std::wstring background_repeat = L"repeat";
    std::wstring background_position = L"0% 0%";
    std::wstring background_size = L"auto";
    std::wstring background_attachment = L"scroll";
    std::wstring background_clip = L"border-box";
    std::wstring background_origin = L"padding-box";
    std::vector<std::wstring> background_layers;

    // Borders
    std::wstring border_top_color = L"currentColor";
    std::wstring border_right_color = L"currentColor";
    std::wstring border_bottom_color = L"currentColor";
    std::wstring border_left_color = L"currentColor";
    std::wstring border_top_style = L"none";
    std::wstring border_right_style = L"none";
    std::wstring border_bottom_style = L"none";
    std::wstring border_left_style = L"none";
    double border_top = 0;
    double border_radius_top_left = 0;
    double border_radius_top_right = 0;
    double border_radius_bottom_right = 0;
    double border_radius_bottom_left = 0;

    // Outline
    std::wstring outline_color = L"currentColor";
    std::wstring outline_style = L"none";
    CssValue outline_width = {0, CssUnit::Pixel, L"0"};
    CssValue outline_offset = {0, CssUnit::Pixel, L"0"};

    // Flexbox
    std::wstring flex_direction = L"row";
    std::wstring flex_wrap = L"nowrap";
    std::wstring justify_content = L"flex-start";
    std::wstring align_items = L"stretch";
    std::wstring align_content = L"stretch";
    std::wstring align_self = L"auto";
    std::wstring justify_self = L"auto";
    double flex_grow = 0.0;
    double flex_shrink = 1.0;
    CssValue flex_basis = {0, CssUnit::Auto, L"auto"};
    int order = 0;

    // Grid
    std::wstring grid_template_columns;
    std::wstring grid_template_rows;
    std::wstring grid_template_areas;
    std::wstring grid_auto_columns = L"auto";
    std::wstring grid_auto_rows = L"auto";
    std::wstring grid_auto_flow = L"row";
    std::wstring grid_column_start = L"auto";
    std::wstring grid_column_end = L"auto";
    std::wstring grid_row_start = L"auto";
    std::wstring grid_row_end = L"auto";
    std::wstring grid_column = L"auto";
    std::wstring grid_row = L"auto";
    std::wstring grid_area = L"auto";
    std::wstring gap;
    std::wstring row_gap;
    std::wstring column_gap;

    // Visual
    std::wstring visibility = L"visible";
    CssValue opacity = {1.0, CssUnit::None, L"1"};
    std::wstring cursor = L"auto";
    std::wstring pointer_events = L"auto";

    // Box Shadow
    std::vector<std::tuple<double,double,double,CssColor>> box_shadows; // offset_x, offset_y, blur_radius, color
    std::wstring box_shadow_string = L"none";

    // Transforms
    std::wstring transform = L"none";
    std::wstring transform_origin = L"50% 50% 0";
    std::wstring transform_style = L"flat";
    std::wstring perspective = L"none";
    std::wstring perspective_origin = L"50% 50%";
    std::wstring backface_visibility = L"visible";

    // Filters
    std::wstring filter = L"none";
    std::wstring backdrop_filter = L"none";

    // Transitions
    std::wstring transition_property = L"all";
    double transition_duration = 0.0;
    double transition_delay = 0.0;
    std::wstring transition_timing_function = L"ease";
    std::vector<std::tuple<std::wstring,double,double,std::wstring>> transitions; // property, duration, delay, timing

    // Animations
    std::wstring animation_name = L"none";
    double animation_duration = 0.0;
    double animation_delay = 0.0;
    int animation_iteration_count = 1;
    std::wstring animation_direction = L"normal";
    std::wstring animation_fill_mode = L"none";
    std::wstring animation_play_state = L"running";
    std::wstring animation_timing_function = L"ease";
    std::vector<std::tuple<std::wstring,double,double,int,std::wstring,std::wstring,std::wstring,std::wstring,std::wstring>> animations;

    // Containment
    std::wstring contain = L"none";
    std::wstring content_visibility = L"visible";
    std::wstring container_type = L"normal";
    std::wstring container_name = L"none";

    // Misc
    std::wstring appearance = L"none";
    std::wstring accent_color = L"auto";
    std::wstring caret_color = L"auto";

    // Inheritance
    bool is_inherited = false;

    // Custom properties
    std::map<std::wstring, CssValue> custom_properties;
};

// Specificity
struct Specificity {
    int a = 0;
    int b = 0;
    int c = 0;

    bool operator<(const Specificity& other) const {
        if (a != other.a) return a < other.a;
        if (b != other.b) return b < other.b;
        return c < other.c;
    }
    bool operator>(const Specificity& other) const { return other < *this; }
    bool operator<=(const Specificity& other) const { return !(other < *this); }
    bool operator>=(const Specificity& other) const { return !(*this < other); }
    bool operator==(const Specificity& other) const {
        return a == other.a && b == other.b && c == other.c;
    }
    Specificity operator+(const Specificity& other) const {
        return {a + other.a, b + other.b, c + other.c};
    }
};

// Matched property with cascade metadata
struct CascadeValue {
    std::wstring value;
    bool important = false;
    CascadeOrigin origin = CascadeOrigin::Author;
    Specificity specificity;
    int source_order = 0;
    bool is_inherited_keyword = false;
    bool is_initial_keyword = false;
    bool is_unset_keyword = false;
    bool is_revert_keyword = false;

    bool operator<(const CascadeValue& other) const {
        if (important != other.important) return important < other.important;
        if (origin != other.origin) return static_cast<int>(origin) < static_cast<int>(other.origin);
        if (!(specificity == other.specificity)) return specificity < other.specificity;
        return source_order < other.source_order;
    }
};

// UA default stylesheet
std::vector<std::pair<std::wstring, std::map<std::wstring, std::wstring>>> get_ua_defaults();

// Style Engine
class StyleEngine {
public:
    StyleEngine();

    void load_stylesheet(const std::wstring& css);
    void load_raw_rules(const std::vector<std::wstring>& rules);
    void load_ua_stylesheet();

    ComputedStyle compute_style(const dom::Node* node, const ComputedStyle* parent_style = nullptr,
                                bool is_hovered = false, bool is_focused = false, bool is_active = false,
                                float viewport_width = 1920, float viewport_height = 1080,
                                float device_pixel_ratio = 1.0f);

    void clear_cache();

    // At-rule storage
    const std::map<std::wstring, KeyframesRule>& keyframes() const { return m_keyframes; }
    const std::vector<FontFace>& font_faces() const { return m_font_faces; }

    static bool is_inherited_property(const std::wstring& name);

private:
    std::vector<ParsedRule> m_parsed_rules;
    std::vector<ParsedRule> m_ua_rules;
    std::map<std::wstring, KeyframesRule> m_keyframes;
    std::vector<FontFace> m_font_faces;
    int m_source_order_counter = 0;

    void parse_rules(const std::wstring& css, std::vector<ParsedRule>& out_rules, bool is_ua);

    Specificity calculate_specificity(const std::wstring& selector);
    bool matches_selector(const dom::Node* node, const std::wstring& selector,
                          bool is_hovered = false, bool is_focused = false, bool is_active = false);

    void parse_property(const std::wstring& name, const std::wstring& value, ComputedStyle& style, bool is_important = false);
    void apply_cascade_value(ComputedStyle& style, const std::wstring& prop, const std::wstring& value);
    void compute_initial_values(ComputedStyle& style);
    void resolve_inherited_values(ComputedStyle& style, const ComputedStyle& parent);
    void resolve_current_color(ComputedStyle& style);

    std::vector<CascadeValue> get_cascade_values(const dom::Node* node, const std::wstring& property,
                                                  bool is_hovered, bool is_focused, bool is_active);
    std::map<std::wstring, std::vector<CascadeValue>> get_all_cascade_values(
        const dom::Node* node, bool is_hovered, bool is_focused, bool is_active,
        float viewport_width, float viewport_height, float dpr);
};

} // namespace hre::css
