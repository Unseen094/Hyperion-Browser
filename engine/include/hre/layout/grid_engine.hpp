#pragma once

#include "hre/css/style_engine.hpp"
#include "hre/layout/box_model.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>

namespace hre::layout {

struct LayoutNode;

struct GridTrack {
    double size = 0;
    double base_size = 0;
    double growth_limit = 0;
    bool is_flexible = false;
    double flex_factor = 0;
    bool is_auto = false;
    bool is_min_content = false;
    bool is_max_content = false;
    bool is_fit_content = false;
    double fit_content_limit = 0;
    double min_content_size = 0;
    double max_content_size = 0;
    bool infinity_growth = false;
    bool is_repeat = false;
    int repeat_count = 1;
};

struct GridLine {
    double position = 0;
    double start = 0;
    double end = 0;
    std::wstring name;
};

struct GridArea {
    std::wstring name;
    int row_start = 0;
    int row_end = 0;
    int col_start = 0;
    int col_end = 0;
};

struct GridCell {
    int row = 0;
    int col = 0;
    std::shared_ptr<LayoutNode> item;
};

struct GridContainer {
    std::vector<GridTrack> row_tracks;
    std::vector<GridTrack> col_tracks;
    std::vector<GridLine> row_lines;
    std::vector<GridLine> col_lines;
    std::vector<std::vector<std::shared_ptr<LayoutNode>>> cells; // [row][col]
    std::map<std::wstring, GridArea> named_areas;

    double total_width = 0;
    double total_height = 0;

    double gap_row = 0;
    double gap_col = 0;

    int explicit_rows = 0;
    int explicit_cols = 0;
    int implicit_rows = 0;
    int implicit_cols = 0;

    std::wstring auto_flow = L"row";
};

class GridEngine {
public:
    GridEngine() = default;

    void layout_grid_container(
        std::shared_ptr<LayoutNode> container,
        double available_width,
        double available_height
    );

private:
    // Track sizing
    void resolve_track_sizes(GridContainer& grid, double available_width, double available_height);
    void calculate_track_base_sizes(std::vector<GridTrack>& tracks, const std::wstring& template_str,
                                     double available_size, const std::wstring& auto_str);
    void initialize_tracks(GridContainer& grid, const css::ComputedStyle& style, double avail_w, double avail_h);
    void resolve_intrinsic_tracks(GridContainer& grid, double available_width, double available_height);
    void distribute_flexible_space(std::vector<GridTrack>& tracks, double remaining_space);
    void resolve_content_based_sizes(GridContainer& grid, std::shared_ptr<LayoutNode> container);
    void maximize_tracks(GridContainer& grid, double available_width, double available_height);
    void expand_flexible_tracks(GridContainer& grid, double available_width, double available_height);
    void stretch_auto_tracks(GridContainer& grid, double available_width, double available_height);

    // Parsing
    void parse_track_list(const std::wstring& template_str, std::vector<GridTrack>& tracks, double available_size);
    void parse_track_token(const std::wstring& token, GridTrack& track, double available_size);
    double parse_gap_value(const std::wstring& value, double base_size);

    // Item placement
    void place_grid_items(std::shared_ptr<LayoutNode> container, GridContainer& grid);
    void place_auto_item(std::shared_ptr<LayoutNode> item, GridContainer& grid, int& row_cursor, int& col_cursor);
    void place_explicit_item(std::shared_ptr<LayoutNode> item, GridContainer& grid);
    void parse_grid_position(const std::wstring& pos, int& start, int& end, int implicit_size);
    void ensure_grid_capacity(GridContainer& grid, int max_row, int max_col);

    // Alignment
    void align_grid_item(std::shared_ptr<LayoutNode> item, const GridTrack& row, const GridTrack& col, const GridContainer& grid);

    // Helpers
    double fit_content_size(const GridTrack& track, double available);
};

} // namespace hre::layout
