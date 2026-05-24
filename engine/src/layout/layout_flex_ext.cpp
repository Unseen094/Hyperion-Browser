#include "hre/layout/flexbox_engine.hpp"
#include "hre/layout/layout_engine.hpp"
#include "hre/layout/box_model.hpp"
#include <algorithm>
#include <cmath>

namespace hre::layout {

static bool has_flex_gap(const css::ComputedStyle& style) {
    return !style.row_gap.empty() || !style.column_gap.empty() || !style.grid_gap.empty();
}

static double parse_gap_value(const std::wstring& gap, double base_size) {
    if (gap.empty()) return 0.0;
    std::wstring trimmed = gap;
    size_t end = trimmed.find_first_of(L" \t");
    if (end != std::wstring::npos) trimmed = trimmed.substr(0, end);

    if (trimmed.size() > 2 && trimmed.substr(trimmed.size() - 2) == L"px") {
        return std::stod(trimmed.substr(0, trimmed.size() - 2));
    }
    if (trimmed.back() == L'%') {
        return std::stod(trimmed.substr(0, trimmed.size() - 1)) / 100.0 * base_size;
    }
    if (trimmed.size() > 2 && trimmed.substr(trimmed.size() - 2) == L"em") {
        return std::stod(trimmed.substr(0, trimmed.size() - 2)) * base_size;
    }
    if (trimmed.size() > 3 && trimmed.substr(trimmed.size() - 3) == L"rem") {
        return std::stod(trimmed.substr(0, trimmed.size() - 3)) * 16.0;
    }
    return 0.0;
}

static void apply_flex_gap(FlexLine& line, const css::ComputedStyle& style, bool is_row, double container_main_size) {
    if (!has_flex_gap(style)) return;

    double gap = 0.0;
    if (is_row) {
        gap = parse_gap_value(style.column_gap.empty() ? style.grid_gap : style.column_gap, container_main_size);
    } else {
        gap = parse_gap_value(style.row_gap.empty() ? style.grid_gap : style.row_gap, container_main_size);
    }

    if (gap <= 0.0 || line.items.size() <= 1) return;

    double total_gap = gap * (line.items.size() - 1);
    double available = container_main_size - total_gap;

    double item_sizes = 0.0;
    for (const auto& item : line.items) {
        item_sizes += is_row ? item.node->content_box.width : item.node->content_box.height;
    }

    if (item_sizes <= 0.0) return;
    double scale = available / item_sizes;

    double offset = 0.0;
    for (auto& item : line.items) {
        if (is_row) {
            item.node->content_box.width *= scale;
            item.main_offset = offset;
            offset += item.node->content_box.width + gap;
        } else {
            item.node->content_box.height *= scale;
            item.main_offset = offset;
            offset += item.node->content_box.height + gap;
        }
    }
}

static void apply_align_content_space_evenly(FlexLine& line, double cross_size, double container_cross_size) {
    if (container_cross_size <= cross_size) return;
    double free_space = container_cross_size - cross_size;
    double gap = free_space / (line.items.size() + 1);
    double offset = gap;
    for (auto& item : line.items) {
        item.cross_offset += offset;
        offset += gap + item.node->content_box.height;
    }
}

static double get_min_height_auto(const std::shared_ptr<LayoutNode>& container, double available_height) {
    const auto& style = container->style;
    if (style.display == L"flex" || style.display == L"inline-flex") {
        double content_height = 0.0;
        for (const auto& child : container->children) {
            content_height = (std::max)(content_height,
                child->content_box.y + child->content_box.height);
        }
        return (std::min)(content_height, available_height);
    }
    return 0.0;
}

void apply_flex_gap_to_container(std::shared_ptr<LayoutNode> container, FlexLine& line, bool is_row) {
    apply_flex_gap(line, container->style, is_row,
        is_row ? container->content_box.width : container->content_box.height);
}

void apply_align_content_space_evenly_to_container(std::shared_ptr<LayoutNode> container, FlexLine& line) {
    (void)container;
    apply_align_content_space_evenly(line, line.cross_size, line.cross_size);
}

void apply_min_height_auto_containment(std::shared_ptr<LayoutNode> container, double available_height) {
    double min_h = get_min_height_auto(container, available_height);
    if (min_h > 0.0 && min_h > container->content_box.height) {
        container->content_box.height = min_h;
    }
}

} // namespace hre::layout
