#pragma once

#include "hre/css/style_engine.hpp"
#include "hre/layout/box_model.hpp"
#include <vector>
#include <memory>
#include <string>

namespace hre::layout {

struct LayoutNode;

enum class FlexDirection { Row, RowReverse, Column, ColumnReverse };
enum class FlexWrap { NoWrap, Wrap, WrapReverse };
enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignItems { Stretch, FlexStart, FlexEnd, Center, Baseline, Auto };
enum class AlignContent { Stretch, FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround };

struct FlexItem {
    std::shared_ptr<LayoutNode> node;
    double flex_grow = 0.0;
    double flex_shrink = 1.0;
    css::CssValue flex_basis;
    double hypothetical_main_size = 0.0;
    double hypothetical_cross_size = 0.0;
    double main_offset = 0.0;
    double cross_offset = 0.0;
    double final_main_size = 0.0;
    double final_cross_size = 0.0;
    double min_main_size = 0.0;
    double max_main_size = 1e9;
    double margin_main_start = 0.0;
    double margin_main_end = 0.0;
    double margin_cross_start = 0.0;
    double margin_cross_end = 0.0;
    int order = 0;
    bool frozen = false;
};

struct FlexLine {
    double main_size = 0;
    double cross_size = 0;
    std::vector<FlexItem> items;
};

class FlexboxEngine {
public:
    FlexboxEngine() = default;

    void layout_flex_container(
        std::shared_ptr<LayoutNode> container,
        double available_width,
        double available_height
    );

private:
    double compute_hypothetical_main_size(const FlexItem& item, double container_main_size, bool is_row);
    void resolve_flexible_lengths(FlexLine& line, double available_main_size, bool is_row);
    void align_items_on_main_axis(FlexLine& line, double container_main_size, FlexDirection dir, JustifyContent jc, double gap);
    void align_cross_axis(FlexLine& line, double container_cross_size, FlexDirection dir, AlignItems ai, AlignContent ac, double gap, double& cross_offset);
    void position_flex_items(FlexLine& line, double cross_start, bool is_row, FlexDirection dir);
    void compute_flex_item_margins(FlexItem& item, double container_main_size, double container_cross_size, bool is_row);
    double resolve_cross_size(const std::shared_ptr<LayoutNode>& node, bool is_row);
    double resolve_gap(const css::ComputedStyle& style, bool is_row_gap, bool is_row, double container_size);

    bool is_row_direction(const FlexDirection& dir);
    double get_main_size(const LayoutRect& rect, bool is_row);
    double get_cross_size(const LayoutRect& rect, bool is_row);
    void set_main_size(LayoutRect& rect, double val, bool is_row);
    void set_cross_size(LayoutRect& rect, double val, bool is_row);
    double get_main_margin(const css::ComputedStyle& style, bool is_row, bool start);
    double get_cross_margin(const css::ComputedStyle& style, bool is_row, bool start);

    FlexDirection parse_flex_direction(const std::wstring& value);
    FlexWrap parse_flex_wrap(const std::wstring& value);
    JustifyContent parse_justify_content(const std::wstring& value);
    AlignItems parse_align_items(const std::wstring& value);
    AlignContent parse_align_content(const std::wstring& value);
};

} // namespace hre::layout
