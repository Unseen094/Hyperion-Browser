#include "hre/layout/grid_engine.hpp"
#include "hre/css/style_engine.hpp"
#include "hre/layout/layout_engine.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace hre::layout {

static std::vector<std::wstring> split_whitespace(const std::wstring& str) {
    std::wistringstream ss(str);
    std::wstring token;
    std::vector<std::wstring> result;
    while (ss >> token) result.push_back(token);
    return result;
}

static std::wstring trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::wstring> split(const std::wstring& str, wchar_t delim) {
    std::vector<std::wstring> result;
    std::wstring current;
    for (wchar_t c : str) {
        if (c == delim) {
            if (!current.empty()) { result.push_back(current); current.clear(); }
        } else current += c;
    }
    if (!current.empty()) result.push_back(current);
    return result;
}

static double parse_number(const std::wstring& s) {
    std::wstring num;
    for (wchar_t c : s) {
        if ((c >= L'0' && c <= L'9') || c == L'.' || c == L'-' || c == L'+') num += c;
    }
    return num.empty() ? 0.0 : std::stod(num);
}

double GridEngine::parse_gap_value(const std::wstring& value, double base_size) {
    if (value.empty()) return 0.0;
    std::wstring s = value;
    if (s.back() == L'p' && s.size() > 2 && s[s.size()-2] == L'x') {
        return parse_number(s);
    }
    if (s.back() == L'%') return parse_number(s) / 100.0 * base_size;
    if (s.size() >= 2 && s.substr(s.size()-2) == L"em") return parse_number(s) * 16.0;
    return parse_number(s);
}

void GridEngine::parse_track_token(const std::wstring& token, GridTrack& track, double available_size) {
    if (token.empty()) return;

    // Handle repeat()
    if (token.find(L"repeat(") == 0) {
        size_t paren = token.find(L'(');
        std::wstring inner = token.substr(paren + 1, token.size() - paren - 2);
        size_t comma = inner.find(L',');
        if (comma == std::wstring::npos) return;
        std::wstring count_str = trim(inner.substr(0, comma));
        std::wstring track_str = trim(inner.substr(comma + 1));
        int count = 1;
        if (count_str == L"auto-fill" || count_str == L"auto-fit") {
            track.repeat_count = -1; // will be resolved later
        } else {
            count = (int)parse_number(count_str);
            track.repeat_count = count;
        }
        track.is_repeat = true;
        parse_track_token(track_str, track, available_size);
        return;
    }

    // Handle minmax()
    if (token.find(L"minmax(") == 0) {
        size_t paren = token.find(L'(');
        std::wstring inner = token.substr(paren + 1, token.size() - paren - 2);
        size_t comma = inner.find(L',');
        if (comma == std::wstring::npos) return;
        std::wstring min_str = trim(inner.substr(0, comma));
        std::wstring max_str = trim(inner.substr(comma + 1));
        // Parse min
        parse_track_token(min_str, track, available_size);
        double min_size = track.size;
        bool min_flex = track.is_flexible;
        // Parse max - reset track
        track = GridTrack();
        parse_track_token(max_str, track, available_size);
        if (track.is_flexible) {
            track.is_flexible = true;
            track.flex_factor = track.flex_factor;
            if (min_flex) track.size = 0;
            else track.size = min_size;
            track.growth_limit = track.infinity_growth ? 1e9 : track.size;
        } else {
            track.size = std::max(min_size, track.size);
        }
        return;
    }

    // Handle fit-content()
    if (token.find(L"fit-content(") == 0) {
        size_t paren = token.find(L'(');
        std::wstring inner = token.substr(paren + 1, token.size() - paren - 2);
        track.is_fit_content = true;
        track.fit_content_limit = parse_number(trim(inner));
        track.size = 0;
        return;
    }

    // Handle min-content
    if (token == L"min-content") {
        track.is_min_content = true;
        track.size = 0;
        return;
    }

    // Handle max-content
    if (token == L"max-content") {
        track.is_max_content = true;
        track.size = 0;
        return;
    }

    // Handle auto
    if (token == L"auto") {
        track.is_auto = true;
        track.size = 0;
        return;
    }

    if (token.back() == L'%') {
        track.size = parse_number(token) / 100.0 * available_size;
        track.is_flexible = false;
    } else if (token.size() >= 2 && token.substr(token.size()-2) == L"px") {
        track.size = parse_number(token);
        track.is_flexible = false;
    } else if (token.size() >= 2 && token.substr(token.size()-2) == L"fr") {
        track.flex_factor = parse_number(token);
        track.is_flexible = true;
    } else if (token.back() == L'%') {
        track.size = parse_number(token) / 100.0 * available_size;
    } else if (token.back() == L'v' || (token.size() > 1 && token[token.size()-1] == L'w') || 
               (token.size() > 1 && token[token.size()-1] == L'h')) {
        track.size = parse_number(token);
    } else {
        track.size = parse_number(token);
    }
    track.base_size = track.size;
}

void GridEngine::parse_track_list(const std::wstring& template_str, std::vector<GridTrack>& tracks, double available_size) {
    tracks.clear();
    if (template_str.empty() || template_str == L"none") {
        GridTrack track;
        track.is_auto = true;
        track.size = 0;
        tracks.push_back(track);
        return;
    }

    // Split by space, handling function parentheses
    std::vector<std::wstring> tokens;
    std::wstring current;
    int depth = 0;
    for (wchar_t c : template_str) {
        if (c == L'(') depth++;
        else if (c == L')') depth--;
        if (c == L' ' && depth == 0) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
        } else if (c == L'\t' || c == L'\n') {
            if (!current.empty() && depth == 0) { tokens.push_back(current); current.clear(); }
        } else current += c;
    }
    if (!current.empty()) tokens.push_back(current);

    for (const auto& token : tokens) {
        GridTrack track;
        parse_track_token(token, track, available_size);
        if (track.is_repeat && track.repeat_count > 0) {
            for (int i = 0; i < track.repeat_count; i++) {
                GridTrack rt = track;
                rt.is_repeat = false;
                rt.repeat_count = 1;
                tracks.push_back(rt);
            }
        } else if (track.is_repeat && track.repeat_count == -1) {
            // auto-fill/auto-fit: add one track for now
            GridTrack rt = track;
            rt.is_repeat = false;
            rt.repeat_count = 1;
            tracks.push_back(rt);
        } else {
            tracks.push_back(track);
        }
    }
}

// Initialize grid from style
void GridEngine::initialize_tracks(GridContainer& grid, const css::ComputedStyle& style, double avail_w, double avail_h) {
    grid.gap_col = parse_gap_value(style.column_gap, avail_w);
    grid.gap_row = parse_gap_value(style.row_gap, avail_h);

    parse_track_list(style.grid_template_columns, grid.col_tracks, avail_w);
    parse_track_list(style.grid_template_rows, grid.row_tracks, avail_h);

    // Parse auto tracks
    GridTrack auto_col;
    parse_track_token(style.grid_auto_columns, auto_col, avail_w);
    if (!auto_col.is_flexible && auto_col.size == 0 && !auto_col.is_auto) auto_col.is_auto = true;
    GridTrack auto_row;
    parse_track_token(style.grid_auto_rows, auto_row, avail_h);
    if (!auto_row.is_flexible && auto_row.size == 0 && !auto_row.is_auto) auto_row.is_auto = true;

    // Store auto flow direction
    grid.auto_flow = style.grid_auto_flow;

    // Parse grid template areas
    if (!style.grid_template_areas.empty() && style.grid_template_areas != L"none") {
        auto rows = split(style.grid_template_areas, L'"');
        for (size_t r = 0; r < rows.size(); r++) {
            std::wstring row = trim(rows[r]);
            if (row.empty()) continue;
            auto col_names = split_whitespace(row);
            for (size_t c = 0; c < col_names.size(); c++) {
                if (col_names[c] != L".") {
                    auto& area = grid.named_areas[col_names[c]];
                    area.name = col_names[c];
                    if (area.row_start == 0 && area.col_start == 0) {
                        area.row_start = (int)r;
                        area.col_start = (int)c;
                    }
                    area.row_end = (int)(r + 1);
                    area.col_end = (int)(c + 1);
                }
            }
        }
    }
}

void GridEngine::calculate_track_base_sizes(std::vector<GridTrack>& tracks, const std::wstring& template_str,
                                             double available_size, const std::wstring& auto_str) {
    parse_track_list(template_str, tracks, available_size);
}

void GridEngine::resolve_intrinsic_tracks(GridContainer& grid, double available_width, double available_height) {
    // Resolve intrinsic sizes (min-content, max-content, auto)
    for (auto& track : grid.col_tracks) {
        if (track.is_min_content) track.size = 0;
        else if (track.is_max_content) track.size = 200; // approximate
        else if (track.is_auto) track.size = 100; // default minimum
        else if (track.is_fit_content) track.size = std::min(track.size, track.fit_content_limit);
    }
    for (auto& track : grid.row_tracks) {
        if (track.is_min_content) track.size = 0;
        else if (track.is_max_content) track.size = 20; // approximate line height
        else if (track.is_auto) track.size = 20;
    }
}

void GridEngine::maximize_tracks(GridContainer& grid, double available_width, double available_height) {
    // Distribute remaining space equally
}

void GridEngine::expand_flexible_tracks(GridContainer& grid, double available_width, double available_height) {
    // Phase: resolve fr units
    double total_flex_col = 0;
    double used_col = 0;
    for (const auto& t : grid.col_tracks) {
        if (t.is_flexible) total_flex_col += t.flex_factor;
        else used_col += t.size;
    }
    double gaps_col = grid.col_tracks.size() > 0 ? (grid.col_tracks.size() - 1) * grid.gap_col : 0;
    double remaining_col = available_width - used_col - gaps_col;
    if (remaining_col < 0) remaining_col = 0;
    if (total_flex_col > 0) {
        for (auto& t : grid.col_tracks) {
            if (t.is_flexible) t.size = (t.flex_factor / total_flex_col) * remaining_col;
        }
    }

    double total_flex_row = 0;
    double used_row = 0;
    for (const auto& t : grid.row_tracks) {
        if (t.is_flexible) total_flex_row += t.flex_factor;
        else used_row += t.size;
    }
    double gaps_row = grid.row_tracks.size() > 0 ? (grid.row_tracks.size() - 1) * grid.gap_row : 0;
    double remaining_row = available_height - used_row - gaps_row;
    if (remaining_row < 0) remaining_row = 0;
    if (total_flex_row > 0) {
        for (auto& t : grid.row_tracks) {
            if (t.is_flexible) t.size = (t.flex_factor / total_flex_row) * remaining_row;
        }
    }
}

void GridEngine::stretch_auto_tracks(GridContainer& grid, double available_width, double available_height) {
    // Stretch auto-sized tracks to fill remaining space
}

void GridEngine::distribute_flexible_space(std::vector<GridTrack>& tracks, double remaining_space) {
    double total_flex = 0;
    for (const auto& t : tracks) {
        if (t.is_flexible) total_flex += t.flex_factor > 0 ? t.flex_factor : 1.0;
    }
    if (total_flex <= 0) return;
    for (auto& t : tracks) {
        if (t.is_flexible) {
            double factor = t.flex_factor > 0 ? t.flex_factor : 1.0;
            t.size = (factor / total_flex) * remaining_space;
        }
    }
}

void GridEngine::resolve_track_sizes(GridContainer& grid, double available_width, double available_height) {
    // Algorithm per CSS Grid spec
    // 1. Initialize tracks
    resolve_intrinsic_tracks(grid, available_width, available_height);
    // 2. Resolve content-based sizes
    // (skipped - requires item content measurement)
    // 3. Maximize tracks
    maximize_tracks(grid, available_width, available_height);
    // 4. Expand flexible tracks
    expand_flexible_tracks(grid, available_width, available_height);
    // 5. Stretch auto tracks
    stretch_auto_tracks(grid, available_width, available_height);

    // Compute line positions
    double x = 0;
    grid.col_lines.clear();
    for (auto& col : grid.col_tracks) {
        GridLine line;
        line.position = x;
        line.start = x;
        line.end = x + col.size;
        grid.col_lines.push_back(line);
        x += col.size + grid.gap_col;
    }
    grid.total_width = x;

    double y = 0;
    grid.row_lines.clear();
    for (auto& row : grid.row_tracks) {
        GridLine line;
        line.position = y;
        line.start = y;
        line.end = y + row.size;
        grid.row_lines.push_back(line);
        y += row.size + grid.gap_row;
    }
    grid.total_height = y;
}

void GridEngine::parse_grid_position(const std::wstring& pos, int& start, int& end, int implicit_size) {
    start = 1; end = 0;
    if (pos.empty() || pos == L"auto") return;

    size_t slash = pos.find(L'/');
    if (slash != std::wstring::npos) {
        std::wstring s = trim(pos.substr(0, slash));
        std::wstring e = trim(pos.substr(slash + 1));
        if (s == L"auto") start = -1;
        else start = (int)parse_number(s);
        if (e == L"auto") end = -1;
        else if (e.find(L"span") == 0) {
            int span = (int)parse_number(e);
            end = start > 0 ? start + span : 1 + span;
        } else end = (int)parse_number(e);
    } else {
        if (pos.find(L"span") == 0) {
            start = -1;
            end = -1 - (int)parse_number(pos);
        } else if (pos == L"auto") {
            start = -1;
        } else {
            start = (int)parse_number(pos);
            end = start + 1;
        }
    }
}

void GridEngine::ensure_grid_capacity(GridContainer& grid, int max_row, int max_col) {
    if (max_row >= (int)grid.cells.size()) {
        grid.cells.resize(max_row + 1);
    }
    for (auto& row : grid.cells) {
        if (max_col >= (int)row.size()) row.resize(max_col + 1);
    }
}

void GridEngine::place_explicit_item(std::shared_ptr<LayoutNode> item, GridContainer& grid) {
    const auto& style = item->style;
    int row_start = 1, row_end = 0, col_start = 1, col_end = 0;

    parse_grid_position(style.grid_row_start, row_start, row_end, (int)grid.row_tracks.size());
    if (row_end <= row_start) {
        parse_grid_position(style.grid_row, row_start, row_end, (int)grid.row_tracks.size());
    }

    parse_grid_position(style.grid_column_start, col_start, col_end, (int)grid.col_tracks.size());
    if (col_end <= col_start) {
        parse_grid_position(style.grid_column, col_start, col_end, (int)grid.col_tracks.size());
    }

    // Parse grid-area
    if (style.grid_area != L"auto") {
        auto it = grid.named_areas.find(style.grid_area);
        if (it != grid.named_areas.end()) {
            row_start = it->second.row_start + 1;
            row_end = it->second.row_end + 1;
            col_start = it->second.col_start + 1;
            col_end = it->second.col_end + 1;
        }
    }

    // Convert to 0-indexed
    int r = (row_start > 0) ? row_start - 1 : 0;
    int c = (col_start > 0) ? col_start - 1 : 0;
    int r_end = (row_end > 0) ? row_end - 1 : (int)grid.row_tracks.size();
    int c_end = (col_end > 0) ? col_end - 1 : (int)grid.col_tracks.size();

    ensure_grid_capacity(grid, std::max(r, r_end), std::max(c, c_end));
    grid.cells[r][c] = item;
}

void GridEngine::place_auto_item(std::shared_ptr<LayoutNode> item, GridContainer& grid, int& row_cursor, int& col_cursor) {
    const auto& style = item->style;
    int row_start = 1, row_end = 0, col_start = 1, col_end = 0;

    parse_grid_position(style.grid_row_start, row_start, row_end, (int)grid.row_tracks.size());
    parse_grid_position(style.grid_column_start, col_start, col_end, (int)grid.col_tracks.size());

    if (col_start > 0) {
        col_cursor = col_start - 1;
    } else {
        // Find next available column
        while (col_cursor < (int)grid.col_tracks.size() && 
               !grid.cells.empty() && row_cursor < (int)grid.cells.size() &&
               col_cursor < (int)grid.cells[row_cursor].size() &&
               grid.cells[row_cursor][col_cursor]) {
            col_cursor++;
            if (col_cursor >= (int)grid.col_tracks.size()) {
                col_cursor = 0;
                row_cursor++;
            }
        }
    }

    if (grid.auto_flow.find(L"column") != std::wstring::npos) {
        // Column-major placement
        if (row_start > 0) row_cursor = row_start - 1;
        ensure_grid_capacity(grid, row_cursor + 1, std::max(col_cursor + 1, (int)grid.col_tracks.size()));
    }

    int r = (row_start > 0) ? row_start - 1 : row_cursor;
    int c = col_cursor;

    ensure_grid_capacity(grid, r + 1, c + 1);
    grid.cells[r][c] = item;

    col_cursor++;
    if (col_cursor >= (int)grid.col_tracks.size()) {
        col_cursor = 0;
        row_cursor++;
    }
}

void GridEngine::place_grid_items(std::shared_ptr<LayoutNode> container, GridContainer& grid) {
    // First pass: place items with explicit grid positions
    std::vector<std::shared_ptr<LayoutNode>> auto_items;
    for (auto& child : container->children) {
        if (!child) continue;
        const auto& s = child->style;
        bool has_explicit = (s.grid_row_start != L"auto" || s.grid_column_start != L"auto" ||
                             s.grid_area != L"auto" || s.grid_row != L"auto" || s.grid_column != L"auto");
        if (has_explicit) {
            place_explicit_item(child, grid);
        } else {
            auto_items.push_back(child);
        }
    }

    // Second pass: auto-place remaining items
    int row_cursor = 0, col_cursor = 0;
    for (auto& item : auto_items) {
        place_auto_item(item, grid, row_cursor, col_cursor);
    }

    // Dense packing (if auto-flow: dense)
    if (grid.auto_flow.find(L"dense") != std::wstring::npos) {
        // Re-pack to fill gaps
        for (int r = 0; r < (int)grid.cells.size(); r++) {
            for (int c = 0; c < (int)grid.cells[r].size(); c++) {
                if (!grid.cells[r][c]) {
                    for (int r2 = r; r2 < (int)grid.cells.size(); r2++) {
                        for (int c2 = (r2 == r ? c + 1 : 0); c2 < (int)grid.cells[r2].size(); c2++) {
                            if (grid.cells[r2][c2]) {
                                grid.cells[r][c] = grid.cells[r2][c2];
                                grid.cells[r2][c2].reset();
                                goto next_cell;
                            }
                        }
                    }
                }
                next_cell:;
            }
        }
    }

    // Set item positions
    for (int r = 0; r < (int)grid.cells.size() && r < (int)grid.row_tracks.size(); r++) {
        for (int c = 0; c < (int)grid.cells[r].size() && c < (int)grid.col_tracks.size(); c++) {
            if (grid.cells[r][c]) {
                auto& item = grid.cells[r][c];
                double ox = container->box_model.padding_left + grid.col_lines[c].start;
                double oy = container->box_model.padding_top + grid.row_lines[r].start;
                item->content_box.x = ox;
                item->content_box.y = oy;
                item->content_box.width = grid.col_tracks[c].size;
                item->content_box.height = grid.row_tracks[r].size;
                align_grid_item(item, grid.row_tracks[r], grid.col_tracks[c], grid);
            }
        }
    }
}

void GridEngine::align_grid_item(std::shared_ptr<LayoutNode> item, const GridTrack& row, const GridTrack& col, const GridContainer& grid) {
    (void)row; (void)col; (void)grid;
    // Apply justify-self and align-self
    const auto& s = item->style;
    // justify-self: stretch items to fill cell width
    if (s.justify_self == L"stretch" || s.justify_self == L"auto") {
        item->content_box.width = col.size;
    }
    // align-self: stretch items to fill cell height
    if (s.align_self == L"stretch" || s.align_self == L"auto") {
        item->content_box.height = row.size;
    }
}

double GridEngine::fit_content_size(const GridTrack& track, double available) {
    if (!track.is_fit_content) return track.size;
    return std::min(track.size, track.fit_content_limit);
}

void GridEngine::layout_grid_container(std::shared_ptr<LayoutNode> container, double available_width, double available_height) {
    if (!container) return;
    const auto& style = container->style;
    GridContainer grid;

    initialize_tracks(grid, style, available_width, available_height);
    resolve_track_sizes(grid, available_width, available_height);
    place_grid_items(container, grid);
}

} // namespace hre::layout
