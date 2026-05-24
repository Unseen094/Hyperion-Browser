#define NOMINMAX
#include "hre/layout/flexbox_engine.hpp"
#include "hre/layout/layout_engine.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace hre::layout {

FlexDirection FlexboxEngine::parse_flex_direction(const std::wstring& value) {
    if (value == L"row-reverse") return FlexDirection::RowReverse;
    if (value == L"column") return FlexDirection::Column;
    if (value == L"column-reverse") return FlexDirection::ColumnReverse;
    return FlexDirection::Row;
}

FlexWrap FlexboxEngine::parse_flex_wrap(const std::wstring& value) {
    if (value == L"wrap") return FlexWrap::Wrap;
    if (value == L"wrap-reverse") return FlexWrap::WrapReverse;
    return FlexWrap::NoWrap;
}

JustifyContent FlexboxEngine::parse_justify_content(const std::wstring& value) {
    if (value == L"flex-end") return JustifyContent::FlexEnd;
    if (value == L"center") return JustifyContent::Center;
    if (value == L"space-between") return JustifyContent::SpaceBetween;
    if (value == L"space-around") return JustifyContent::SpaceAround;
    if (value == L"space-evenly") return JustifyContent::SpaceEvenly;
    return JustifyContent::FlexStart;
}

AlignItems FlexboxEngine::parse_align_items(const std::wstring& value) {
    if (value == L"flex-end") return AlignItems::FlexEnd;
    if (value == L"center") return AlignItems::Center;
    if (value == L"baseline") return AlignItems::Baseline;
    if (value == L"stretch") return AlignItems::Stretch;
    return AlignItems::Stretch;
}

AlignContent FlexboxEngine::parse_align_content(const std::wstring& value) {
    if (value == L"flex-start") return AlignContent::FlexStart;
    if (value == L"flex-end") return AlignContent::FlexEnd;
    if (value == L"center") return AlignContent::Center;
    if (value == L"space-between") return AlignContent::SpaceBetween;
    if (value == L"space-around") return AlignContent::SpaceAround;
    return AlignContent::Stretch;
}

bool FlexboxEngine::is_row_direction(const FlexDirection& dir) {
    return dir == FlexDirection::Row || dir == FlexDirection::RowReverse;
}

double FlexboxEngine::get_main_size(const LayoutRect& rect, bool is_row) {
    return is_row ? rect.width : rect.height;
}

double FlexboxEngine::get_cross_size(const LayoutRect& rect, bool is_row) {
    return is_row ? rect.height : rect.width;
}

void FlexboxEngine::set_main_size(LayoutRect& rect, double val, bool is_row) {
    if (is_row) rect.width = val;
    else rect.height = val;
}

void FlexboxEngine::set_cross_size(LayoutRect& rect, double val, bool is_row) {
    if (is_row) rect.height = val;
    else rect.width = val;
}

double FlexboxEngine::get_main_margin(const css::ComputedStyle& style, bool is_row, bool start) {
    if (is_row) return start ? style.margin_left.to_pixels(0) : style.margin_right.to_pixels(0);
    return start ? style.margin_top.to_pixels(0) : style.margin_bottom.to_pixels(0);
}

double FlexboxEngine::get_cross_margin(const css::ComputedStyle& style, bool is_row, bool start) {
    if (is_row) return start ? style.margin_top.to_pixels(0) : style.margin_bottom.to_pixels(0);
    return start ? style.margin_left.to_pixels(0) : style.margin_right.to_pixels(0);
}

double FlexboxEngine::resolve_cross_size(const std::shared_ptr<LayoutNode>& node, bool is_row) {
    if (is_row) {
        if (node->style.height.unit != css::CssUnit::Auto) return node->style.height.to_pixels(0);
    } else {
        if (node->style.width.unit != css::CssUnit::Auto) return node->style.width.to_pixels(0);
    }
    return 0;
}

double FlexboxEngine::resolve_gap(const css::ComputedStyle& style, bool is_row_gap, bool is_row, double container_size) {
    double gap = 0;
    const std::wstring& gap_str = is_row_gap ? style.row_gap : style.column_gap;
    if (!gap_str.empty() && gap_str != L"normal") {
        try {
            gap = std::stod(gap_str);
        } catch (...) {}
    }
    if (gap <= 0) gap = 0;
    return gap;
}

double FlexboxEngine::compute_hypothetical_main_size(const FlexItem& item, double container_main_size, bool is_row) {
    const auto& s = item.node->style;
    double basis = 0;
    if (s.flex_basis.unit != css::CssUnit::Auto) {
        basis = s.flex_basis.to_pixels(container_main_size);
    } else {
        // Use main-size from width/height
        basis = is_row ? s.width.to_pixels(container_main_size) : s.height.to_pixels(container_main_size);
    }
    if (basis <= 0) basis = 0;
    return basis;
}

void FlexboxEngine::compute_flex_item_margins(FlexItem& item, double container_main_size, double container_cross_size, bool is_row) {
    const auto& s = item.node->style;
    item.margin_main_start = get_main_margin(s, is_row, true);
    item.margin_main_end = get_main_margin(s, is_row, false);
    item.margin_cross_start = get_cross_margin(s, is_row, true);
    item.margin_cross_end = get_cross_margin(s, is_row, false);
}

void FlexboxEngine::resolve_flexible_lengths(FlexLine& line, double available_main_size, bool is_row) {
    double total_flex_grow = 0;
    double total_flex_shrink = 0;
    double initial_total = 0;

    for (auto& item : line.items) {
        total_flex_grow += item.flex_grow;
        total_flex_shrink += item.flex_shrink;
        initial_total += item.hypothetical_main_size + item.margin_main_start + item.margin_main_end;
    }

    double free_space = available_main_size - initial_total;

    if (free_space > 0 && total_flex_grow > 0) {
        // Distribute positive free space
        double space_per_flex = free_space / total_flex_grow;
        for (auto& item : line.items) {
            item.final_main_size = item.hypothetical_main_size + space_per_flex * item.flex_grow;
            item.frozen = true;
        }
    } else if (free_space < 0 && total_flex_shrink > 0) {
        // Distribute negative free space
        double total_scaled_shrink = 0;
        for (auto& item : line.items) {
            total_scaled_shrink += item.flex_shrink * item.hypothetical_main_size;
        }
        if (total_scaled_shrink > 0) {
            double shrink_per_unit = std::abs(free_space) / total_scaled_shrink;
            for (auto& item : line.items) {
                double shrink = shrink_per_unit * item.flex_shrink * item.hypothetical_main_size;
                item.final_main_size = item.hypothetical_main_size - shrink;
                if (item.final_main_size < item.min_main_size) {
                    item.final_main_size = item.min_main_size;
                }
                item.frozen = true;
            }
        }
    } else {
        for (auto& item : line.items) {
            item.final_main_size = item.hypothetical_main_size;
            item.frozen = true;
        }
    }
}

void FlexboxEngine::align_items_on_main_axis(FlexLine& line, double container_main_size, FlexDirection dir, JustifyContent jc, double gap) {
    if (line.items.empty()) return;

    double used_size = 0;
    for (auto& item : line.items) {
        used_size += item.final_main_size + item.margin_main_start + item.margin_main_end;
    }

    double total_gap = gap * (line.items.size() - 1);
    double remaining = container_main_size - used_size - total_gap;

    double start_offset = 0;

    switch (jc) {
    case JustifyContent::Center:
        start_offset = remaining / 2.0;
        break;
    case JustifyContent::FlexEnd:
        start_offset = remaining;
        break;
    case JustifyContent::SpaceBetween:
        if (line.items.size() > 1) {
            double space = remaining / (line.items.size() - 1);
            double offset = 0;
            for (auto& item : line.items) {
                item.main_offset = offset;
                offset += item.final_main_size + item.margin_main_start + item.margin_main_end + space + gap;
            }
            return;
        }
        break;
    case JustifyContent::SpaceAround:
        if (line.items.size() > 0) {
            double space = remaining / line.items.size();
            double offset = space / 2.0;
            for (auto& item : line.items) {
                item.main_offset = offset;
                offset += item.final_main_size + item.margin_main_start + item.margin_main_end + space + gap;
            }
            return;
        }
        break;
    case JustifyContent::SpaceEvenly: {
        double space = remaining / (line.items.size() + 1);
        double offset = space;
        for (auto& item : line.items) {
            item.main_offset = offset;
            offset += item.final_main_size + item.margin_main_start + item.margin_main_end + space + gap;
        }
        return;
    }
    default:
        break;
    }

    double offset = start_offset;
    for (auto& item : line.items) {
        item.main_offset = offset + item.margin_main_start;
        offset += item.final_main_size + item.margin_main_start + item.margin_main_end + gap;
    }

    // Handle row-reverse / column-reverse
    if (dir == FlexDirection::RowReverse || dir == FlexDirection::ColumnReverse) {
        for (auto& item : line.items) {
            item.main_offset = container_main_size - item.main_offset - item.final_main_size;
        }
        std::reverse(line.items.begin(), line.items.end());
    }
}

void FlexboxEngine::align_cross_axis(FlexLine& line, double container_cross_size, FlexDirection dir, AlignItems ai, AlignContent ac, double gap, double& cross_offset) {
    double max_cross = 0;

    // Determine cross sizes
    for (auto& item : line.items) {
        if (ai == AlignItems::Stretch && item.node->style.height.unit == css::CssUnit::Auto && is_row_direction(dir)) {
            item.final_cross_size = line.cross_size;
        } else if (ai == AlignItems::Stretch && item.node->style.width.unit == css::CssUnit::Auto && !is_row_direction(dir)) {
            item.final_cross_size = line.cross_size;
        } else if (item.final_cross_size <= 0) {
            BoxModel item_box = calculate_box_model(item.node->style, item.final_main_size, container_cross_size);
            item.final_cross_size = item.node->content_box.height > 0 ? item.node->content_box.height : item_box.content_height;
        }
        max_cross = std::max(max_cross, item.final_cross_size);
    }

    if (line.cross_size <= 0) {
        line.cross_size = max_cross;
    }
    if (line.cross_size <= 0) line.cross_size = 10;

    // Align items within cross axis
    for (auto& item : line.items) {
        double item_cross = item.final_cross_size + item.margin_cross_start + item.margin_cross_end;
        switch (ai) {
        case AlignItems::FlexEnd:
            item.cross_offset = line.cross_size - item_cross + item.margin_cross_start;
            break;
        case AlignItems::Center:
            item.cross_offset = (line.cross_size - item_cross) / 2.0 + item.margin_cross_start;
            break;
        case AlignItems::Baseline:
            item.cross_offset = item.margin_cross_start;
            break;
        default: // Stretch, FlexStart
            item.cross_offset = item.margin_cross_start;
            break;
        }
    }
}

void FlexboxEngine::position_flex_items(FlexLine& line, double cross_start, bool is_row, FlexDirection dir) {
    for (auto& item : line.items) {
        if (is_row) {
            item.node->content_box.x = item.main_offset;
            item.node->content_box.y = cross_start + item.cross_offset;
            if (dir == FlexDirection::RowReverse) {
                item.node->content_box.x -= item.final_main_size;
            }
            item.node->content_box.width = item.final_main_size;
            item.node->content_box.height = item.final_cross_size;
        } else {
            item.node->content_box.y = item.main_offset;
            item.node->content_box.x = cross_start + item.cross_offset;
            if (dir == FlexDirection::ColumnReverse) {
                item.node->content_box.y -= item.final_main_size;
            }
            item.node->content_box.height = item.final_main_size;
            item.node->content_box.width = item.final_cross_size;
        }
    }
}

void FlexboxEngine::layout_flex_container(
    std::shared_ptr<LayoutNode> container,
    double available_width,
    double available_height
) {
    if (!container) return;

    const auto& style = container->style;
    FlexDirection dir = parse_flex_direction(style.flex_direction);
    FlexWrap wrap = parse_flex_wrap(style.flex_wrap);
    JustifyContent jc = parse_justify_content(style.justify_content);
    AlignItems ai = parse_align_items(style.align_items);
    AlignContent ac = parse_align_content(style.align_content);

    bool is_row = is_row_direction(dir);
    double container_main_size = is_row ? available_width : available_height;
    double container_cross_size = is_row ? available_height : available_width;

    if (container_main_size <= 0) container_main_size = available_width > 0 ? available_width : 800;
    if (container_cross_size <= 0) container_cross_size = available_height > 0 ? available_height : 600;

    // Per-item gap
    double row_gap = resolve_gap(style, true, is_row, container_main_size);
    double col_gap = resolve_gap(style, false, is_row, container_main_size);
    double main_gap = is_row ? col_gap : row_gap;
    double cross_gap = is_row ? row_gap : col_gap;

    // Step 1: Collect flex items, sort by order
    std::vector<FlexItem> all_items;
    for (auto& child : container->children) {
        if (!child) continue;
        FlexItem item;
        item.node = child;
        item.flex_grow = child->style.flex_grow;
        item.flex_shrink = child->style.flex_shrink;
        item.flex_basis = child->style.flex_basis;
        item.order = child->style.order;

        if (item.flex_basis.unit == css::CssUnit::Auto) {
            item.hypothetical_main_size = is_row ?
                child->style.width.to_pixels(container_main_size) :
                child->style.height.to_pixels(container_main_size);
        } else {
            item.hypothetical_main_size = item.flex_basis.to_pixels(container_main_size);
        }
        if (item.hypothetical_main_size <= 0) {
            if (is_row && child->style.width.unit != css::CssUnit::Auto) {
                item.hypothetical_main_size = child->style.width.to_pixels(container_main_size);
            } else if (!is_row && child->style.height.unit != css::CssUnit::Auto) {
                item.hypothetical_main_size = child->style.height.to_pixels(container_main_size);
            }
        }

        item.min_main_size = is_row ?
            child->style.min_width.to_pixels(container_main_size) :
            child->style.min_height.to_pixels(container_main_size);
        item.max_main_size = is_row ?
            child->style.max_width.to_pixels(container_main_size) :
            child->style.max_height.to_pixels(container_main_size);
        if (item.max_main_size <= 0) item.max_main_size = 1e9;

        compute_flex_item_margins(item, container_main_size, container_cross_size, is_row);
        all_items.push_back(item);
    }

    std::sort(all_items.begin(), all_items.end(), [](const FlexItem& a, const FlexItem& b) {
        return a.order < b.order;
    });

    // Step 2: Collect items into flex lines
    std::vector<FlexLine> lines;
    FlexLine current_line;
    double line_main_used = 0;

    for (auto& item : all_items) {
        double item_main = item.hypothetical_main_size + item.margin_main_start + item.margin_main_end;
        bool new_line = false;

        if (wrap != FlexWrap::NoWrap) {
            if (!current_line.items.empty() && line_main_used + main_gap + item_main > container_main_size) {
                new_line = true;
            }
        }

        if (new_line) {
            if (!current_line.items.empty()) {
                current_line.main_size = line_main_used;
                lines.push_back(current_line);
            }
            current_line = FlexLine();
            line_main_used = 0;
        }

        current_line.items.push_back(item);
        line_main_used += item_main + (current_line.items.size() > 1 ? main_gap : 0);
    }

    if (!current_line.items.empty()) {
        current_line.main_size = line_main_used;
        lines.push_back(current_line);
    }

    // Step 3: Resolve flexible lengths for each line
    for (auto& line : lines) {
        resolve_flexible_lengths(line, container_main_size, is_row);
    }

    // Step 4: Determine cross size of each line
    for (auto& line : lines) {
        double max_item_cross = 0;
        for (auto& item : line.items) {
            double item_cross = resolve_cross_size(item.node, is_row);
            BoxModel item_box = calculate_box_model(item.node->style, item.final_main_size, container_cross_size);
            if (item_cross <= 0) {
                item_cross = item_box.content_height > 0 ? item_box.content_height : 20;
            }
            item.final_cross_size = item_cross;
            item.hypothetical_cross_size = item_cross;
            max_item_cross = std::max(max_item_cross, item_cross + item.margin_cross_start + item.margin_cross_end);
        }
        if (line.cross_size <= 0) {
            line.cross_size = max_item_cross;
        }
        if (line.cross_size <= 0) line.cross_size = 10;
    }

    // Step 5: Align content (cross-axis distribution of lines)
    double total_cross_used = 0;
    double max_cross_line = 0;
    for (auto& line : lines) {
        total_cross_used += line.cross_size;
        max_cross_line = std::max(max_cross_line, line.cross_size);
    }
    total_cross_used += cross_gap * (lines.size() - 1);

    double cross_remaining = container_cross_size - total_cross_used;
    double cross_start = 0;

    switch (ac) {
    case AlignContent::FlexEnd:
        cross_start = cross_remaining;
        break;
    case AlignContent::Center:
        cross_start = cross_remaining / 2.0;
        break;
    case AlignContent::SpaceBetween:
        if (lines.size() > 1) {
            double space = cross_remaining / (lines.size() - 1);
            double offset = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                double line_offset = offset;
                align_cross_axis(lines[i], container_cross_size, dir, ai, ac, cross_gap, line_offset);
                for (auto& item : lines[i].items) {
                    item.cross_offset += line_offset;
                }
                offset += lines[i].cross_size + space + cross_gap;
            }
            // Position items
            double y_offset = wrap == FlexWrap::WrapReverse ? container_cross_size : 0;
            double y_dir = wrap == FlexWrap::WrapReverse ? -1.0 : 1.0;
            for (auto& line : lines) {
                position_flex_items(line, y_offset, is_row, dir);
                y_offset += line.cross_size * y_dir + cross_gap;
            }
            return;
        }
        break;
    case AlignContent::SpaceAround:
        if (lines.size() > 0) {
            double space = cross_remaining / lines.size();
            cross_start = space / 2.0;
        }
        break;
    default:
        // Stretch: distribute remaining space equally
        if (lines.size() > 0 && cross_remaining > 0) {
            double extra = cross_remaining / lines.size();
            for (auto& line : lines) line.cross_size += extra;
        }
        break;
    }

    // Step 6: Align items within each line and position
    double y_offset = cross_start;
    if (wrap == FlexWrap::WrapReverse) {
        y_offset = container_cross_size - cross_start;
    }
    double y_dir = wrap == FlexWrap::WrapReverse ? -1.0 : 1.0;

    for (auto& line : lines) {
        align_cross_axis(line, container_cross_size, dir, ai, ac, cross_gap, y_offset);
        position_flex_items(line, y_offset, is_row, dir);

        // Align items on main axis
        align_items_on_main_axis(line, container_main_size, dir, jc, main_gap);

        y_offset += line.cross_size * y_dir + cross_gap;
    }

    // Step 7: Compute container dimensions
    double total_main = container_main_size;
    double total_cross = 0;
    for (auto& line : lines) total_cross += line.cross_size;
    total_cross += cross_gap * std::max(0, (int)lines.size() - 1);

    if (is_row) {
        container->content_box.width = total_main;
        container->content_box.height = std::max(total_cross, container_cross_size);
    } else {
        container->content_box.height = total_main;
        container->content_box.width = std::max(total_cross, container_cross_size);
    }
}

} // namespace hre::layout
