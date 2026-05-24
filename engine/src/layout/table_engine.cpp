#define NOMINMAX
#include "hre/layout/table_engine.hpp"
#include "hre/layout/layout_engine.hpp"
#include <algorithm>
#include <cmath>

namespace hre::layout {

void TableEngine::resolve_anonymous_objects(std::shared_ptr<LayoutNode> table) {
    std::vector<std::shared_ptr<LayoutNode>> children;
    bool prev_is_cell = false;
    bool prev_is_row = false;
    bool prev_is_section = false;
    int anonymous_count = 0;

    std::shared_ptr<LayoutNode> anon_row;
    std::shared_ptr<LayoutNode> anon_section;

    // Insert anonymous wrapper objects per CSS 2.2 spec
    bool has_changes = false;
    do {
        has_changes = false;
        children.clear();
        for (auto& child : table->children) {
            children.push_back(child);
        }
        table->children.clear();

        anon_row.reset();
        anon_section.reset();

        for (size_t i = 0; i < children.size(); ++i) {
            auto& child = children[i];
            auto display = child->style.display;

            if (display == L"table-caption" || display == L"table-column-group" ||
                display == L"table-column") {
                table->children.push_back(child);
                continue;
            }

            if (display == L"table-row") {
                if (anon_section) {
                    table->children.push_back(anon_section);
                    anon_section.reset();
                }
                table->children.push_back(child);
                continue;
            }

            if (display.find(L"table-row-group") != std::wstring::npos ||
                display == L"table-header-group" || display == L"table-footer-group") {
                if (anon_section) {
                    table->children.push_back(anon_section);
                    anon_section.reset();
                }
                table->children.push_back(child);
                continue;
            }

            if (display == L"table-cell") {
                if (!anon_row) {
                    anon_row = std::make_shared<LayoutNode>(nullptr);
                    anon_row->style.display = L"table-row";
                }
                anon_row->children.push_back(child);
                has_changes = true;
                continue;
            }

            // Non-table child - wrap in anonymous row and section
            if (!anon_section) {
                anon_section = std::make_shared<LayoutNode>(nullptr);
                anon_section->style.display = L"table-row-group";
            }
            if (!anon_row) {
                anon_row = std::make_shared<LayoutNode>(nullptr);
                anon_row->style.display = L"table-row";
                anon_section->children.push_back(anon_row);
            }
            anon_row->children.push_back(child);
            has_changes = true;
        }

        if (anon_section) table->children.push_back(anon_section);
        if (anon_row) {
            if (!anon_section) {
                anon_section = std::make_shared<LayoutNode>(nullptr);
                anon_section->style.display = L"table-row-group";
                anon_section->children.push_back(anon_row);
                table->children.push_back(anon_section);
            }
        }

        children = table->children;
    } while (false);
}

void TableEngine::apply_border_collapse(std::shared_ptr<LayoutNode> table) {
    // Simplified border collapse - always applied
    for (auto& section : table->children) {
        for (auto& row : section->children) {
            for (auto& cell : row->children) {
                auto& cs = cell->style;
                if (cs.border_top_width.number < 0) cs.border_top_width.number = 0;
                if (cs.border_bottom_width.number < 0) cs.border_bottom_width.number = 0;
                if (cs.border_left_width.number < 0) cs.border_left_width.number = 0;
                if (cs.border_right_width.number < 0) cs.border_right_width.number = 0;
            }
        }
    }

    // Collapse adjacent cell borders into one
    for (auto& section : table->children) {
        for (size_t ri = 0; ri < section->children.size(); ++ri) {
            auto& row = section->children[ri];
            for (size_t ci = 0; ci < row->children.size(); ++ci) {
                auto& cell = row->children[ci];
                if (ci > 0) {
                    auto& prev_cell = row->children[ci - 1];
                    double max_left = std::max(cell->style.border_left_width.number, prev_cell->style.border_right_width.number);
                    cell->style.border_left_width = css::CssValue{max_left, css::CssUnit::Pixel, L""};
                    prev_cell->style.border_right_width = css::CssValue{0, css::CssUnit::Pixel, L""};
                }
                if (ri > 0) {
                    auto& prev_row = section->children[ri - 1];
                    if (ci < prev_row->children.size()) {
                        auto& prev_cell = prev_row->children[ci];
                        double max_top = std::max(cell->style.border_top_width.number, prev_cell->style.border_bottom_width.number);
                        cell->style.border_top_width = css::CssValue{max_top, css::CssUnit::Pixel, L""};
                        prev_cell->style.border_bottom_width = css::CssValue{0, css::CssUnit::Pixel, L""};
                    }
                }
            }
        }
    }
}

void TableEngine::collect_cells(std::shared_ptr<LayoutNode> table, std::vector<TableCell>& cells, double available_width) {
    int current_row = 0;
    for (auto& section : table->children) {
        auto section_display = section->style.display;
        if (section_display.find(L"table-row-group") == std::wstring::npos &&
            section_display != L"table-header-group" &&
            section_display != L"table-footer-group") {
            continue;
        }
        for (auto& row : section->children) {
            if (row->style.display != L"table-row") continue;
            int current_col = 0;
            for (auto& cell : row->children) {
                if (cell->style.display != L"table-cell") continue;
                TableCell tc;
                tc.node = cell;
                tc.row = current_row;
                tc.col = current_col;
                tc.rowspan = 1;
                tc.colspan = 1;
                tc.rowspan = 1;
                tc.min_width = cell->style.width.to_pixels(available_width);
                cells.push_back(tc);
                current_col += tc.colspan;
            }
            ++current_row;
        }
    }
}

void TableEngine::compute_column_widths(std::vector<TableCell>& cells, double total_width) {
    // Determine number of columns
    int max_col = 0;
    for (auto& c : cells) {
        int end = c.col + c.colspan - 1;
        max_col = std::max(max_col, end);
    }
    int num_cols = max_col + 1;
    if (num_cols <= 0) return;

    std::vector<double> col_widths(num_cols, 0);
    std::vector<double> col_percent(num_cols, 0);
    std::vector<bool> col_fixed(num_cols, false);

    // Phase 1: Apply fixed widths
    for (auto& c : cells) {
        double cell_width = c.min_width;
        if (cell_width > 0 && c.node->style.width.unit == css::CssUnit::Percent) {
            double pct = c.node->style.width.number;
            for (int i = 0; i < c.colspan; ++i) {
                col_percent[c.col + i] = std::max(col_percent[c.col + i], pct / c.colspan);
            }
        } else if (cell_width > 0) {
            double per_col_width = cell_width / c.colspan;
            for (int i = 0; i < c.colspan; ++i) {
                col_widths[c.col + i] = std::max(col_widths[c.col + i], per_col_width);
                col_fixed[c.col + i] = true;
            }
        }
    }

    // Phase 2: Distribute remaining space
    double used_width = 0;
    int auto_cols = 0;
    for (int i = 0; i < num_cols; ++i) {
        if (col_widths[i] <= 0) {
            col_widths[i] = total_width * (col_percent[i] > 0 ? col_percent[i] / 100.0 : 0);
            if (col_widths[i] <= 0) {
                col_widths[i] = 80;
                ++auto_cols;
            }
        }
        used_width += col_widths[i];
    }

    // Distribute remaining space equally
    if (auto_cols > 0 && total_width > used_width) {
        double extra = (total_width - used_width) / auto_cols;
        for (int i = 0; i < num_cols; ++i) {
            if (!col_fixed[i] && col_percent[i] <= 0) {
                col_widths[i] += extra;
            }
        }
    }

    // Resolve cell positions and widths
    std::vector<double> col_start(num_cols, 0);
    double x_pos = 0;
    for (int i = 0; i < num_cols; ++i) {
        col_start[i] = x_pos;
        x_pos += col_widths[i];
    }

    for (auto& c : cells) {
        c.x = col_start[c.col];
        c.width = 0;
        for (int i = 0; i < c.colspan; ++i) {
            c.width += col_widths[c.col + i];
        }
    }
}

void TableEngine::layout_table_caption(std::shared_ptr<LayoutNode> table, double width) {
    for (auto& child : table->children) {
        if (child->style.display == L"table-caption") {
            child->content_box.x = 0;
            child->content_box.y = 0;
            child->content_box.width = width;
            child->content_box.height = child->content_box.height > 0 ? child->content_box.height : 30;
        }
    }
}

void TableEngine::layout_table(
    std::shared_ptr<LayoutNode> table,
    double available_width,
    double available_height
) {
    resolve_anonymous_objects(table);
    apply_border_collapse(table);

    double table_width = table->style.width.to_pixels(available_width);
    if (table_width <= 0) table_width = available_width;

    layout_table_caption(table, table_width);

    // Collect all cells
    std::vector<TableCell> cells;
    collect_cells(table, cells, table_width);

    // Compute column widths
    compute_column_widths(cells, table_width);

    // Layout rows and compute heights
    int current_row = 0;
    double y_pos = 0;
    double caption_height = 0;

    // Account for caption (always top in simplified model)
    for (auto& child : table->children) {
        if (child->style.display == L"table-caption") {
            caption_height = child->content_box.height;
            y_pos = caption_height;
        }
    }

    // Compute row heights (auto from content)
    std::vector<double> row_heights;

    // Compute total rows
    for (size_t ri = 0; ri <= cells.size(); ++ri) {
        int max_row = 0;
        for (auto& c : cells) max_row = std::max(max_row, c.row + c.rowspan);
        if (ri + 1 > row_heights.size()) row_heights.resize(max_row, 0);
    }

    // Determine row heights from cell content
    for (auto& c : cells) {
        BoxModel cell_box = calculate_box_model(c.node->style, c.width, available_height);
        c.node->content_box.width = c.width - cell_box.padding_left - cell_box.padding_right - cell_box.border_left - cell_box.border_right;
        if (c.node->content_box.width <= 0) c.node->content_box.width = c.width;

        double cell_content_height = c.node->content_box.height;
        if (cell_content_height <= 0) cell_content_height = 30;

        double cell_full_height = cell_content_height + cell_box.padding_top + cell_box.padding_bottom + cell_box.border_top + cell_box.border_bottom;

        double per_row_height = cell_full_height / c.rowspan;
        for (int i = 0; i < c.rowspan; ++i) {
            if (c.row + i < (int)row_heights.size()) {
                row_heights[c.row + i] = std::max(row_heights[c.row + i], per_row_height);
            }
        }
    }

    // Position cells
    for (auto& c : cells) {
        // Calculate y position
        double cy = y_pos;
        for (int i = 0; i < c.row; ++i) {
            if (i < (int)row_heights.size()) cy += row_heights[i];
        }

        double cell_full_height = 0;
        for (int i = 0; i < c.rowspan; ++i) {
            if (c.row + i < (int)row_heights.size()) cell_full_height += row_heights[c.row + i];
        }

        BoxModel cell_box = calculate_box_model(c.node->style, c.width, available_height);
        c.node->content_box.x = c.x + cell_box.margin_left + cell_box.border_left + cell_box.padding_left;
        c.node->content_box.y = cy + cell_box.margin_top + cell_box.border_top + cell_box.padding_top;
        c.height = cell_full_height;
    }

    // Compute table height
    double table_height = y_pos;
    for (auto& rh : row_heights) table_height += rh;
    table->content_box.height = table_height + caption_height;
    table->content_box.width = table_width;

    // Caption moved below table
    if (!table->children.empty() && table->children[0]->style.display == L"table-caption") {
        for (auto& child : table->children) {
            if (child->style.display == L"table-caption") {
                child->content_box.y = table_height;
            }
        }
        table->content_box.height += caption_height;
    }
}

double TableEngine::compute_total_width(const std::vector<TableCell>& cells) {
    double total = 0;
    for (auto& c : cells) total += c.width;
    return total;
}

void TableEngine::position_table_sections(const std::vector<TableSection>& sections, double table_y) {
    // Already handled in layout_table
}

} // namespace hre::layout
