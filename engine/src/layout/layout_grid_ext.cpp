#include "hre/layout/grid_engine.hpp"
#include "hre/layout/layout_engine.hpp"
#include "hre/layout/box_model.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <set>

namespace hre::layout {

// ---- Subgrid Support ------------------------------------------------------

struct SubgridContext {
    std::shared_ptr<LayoutNode> subgrid_container;
    std::vector<GridTrack> inherited_col_tracks;
    std::vector<GridTrack> inherited_row_tracks;
    double offset_x = 0.0;
    double offset_y = 0.0;
};

static std::vector<SubgridContext> s_subgrid_stack;

static bool is_subgrid(const std::wstring& template_val) {
    return template_val.find(L"subgrid") != std::wstring::npos;
}

static void push_subgrid_context(std::shared_ptr<LayoutNode> container,
                                  const std::vector<GridTrack>& col_tracks,
                                  const std::vector<GridTrack>& row_tracks)
{
    SubgridContext ctx;
    ctx.subgrid_container = container;
    ctx.inherited_col_tracks = col_tracks;
    ctx.inherited_row_tracks = row_tracks;
    ctx.offset_x = container->content_box.x;
    ctx.offset_y = container->content_box.y;
    s_subgrid_stack.push_back(ctx);
}

static void pop_subgrid_context() {
    if (!s_subgrid_stack.empty()) {
        s_subgrid_stack.pop_back();
    }
}

static bool has_subgrid_ancestor() {
    return !s_subgrid_stack.empty();
}

static std::vector<GridTrack> resolve_subgrid_tracks(
    const std::wstring& template_val,
    const std::vector<GridTrack>& local_tracks)
{
    if (is_subgrid(template_val) && !local_tracks.empty()) {
        return local_tracks;
    }
    return {};
}

// ---- Auto-fill / Auto-fit ------------------------------------------------

struct AutoRepeatContext {
    std::wstring type;
    std::wstring track_pattern;
    int repeat_count = 0;
    bool is_auto_fill = false;
    bool is_auto_fit = false;
};

static AutoRepeatContext parse_auto_repeat(const std::wstring& template_val) {
    AutoRepeatContext ctx;
    size_t auto_pos = template_val.find(L"auto-fill");
    if (auto_pos != std::wstring::npos) {
        ctx.is_auto_fill = true;
        ctx.type = L"auto-fill";
        ctx.repeat_count = 0;
    } else {
        auto_pos = template_val.find(L"auto-fit");
        if (auto_pos != std::wstring::npos) {
            ctx.is_auto_fit = true;
            ctx.type = L"auto-fit";
            ctx.repeat_count = 0;
        }
    }
    return ctx;
}

static int compute_auto_repeat_count(const AutoRepeatContext& ctx, double available_size,
                                      const std::vector<GridTrack>& fixed_tracks, double gap)
{
    if (!ctx.is_auto_fill && !ctx.is_auto_fit) return 0;
    if (fixed_tracks.empty()) return 1;

    double fixed_total = 0.0;
    for (const auto& t : fixed_tracks) {
        if (!t.is_flexible) fixed_total += t.size;
    }

    double remaining = available_size - fixed_total;
    if (remaining <= 0) return 0;

    double avg_track_size = fixed_tracks[0].size;
    if (avg_track_size <= 0) return 1;

    int max_fit = (int)(remaining / (avg_track_size + gap));
    return std::max(1, max_fit);
}

static std::vector<GridTrack> expand_auto_repeated_tracks(
    const std::vector<GridTrack>& base_tracks,
    const AutoRepeatContext& ctx,
    double available_size,
    double gap)
{
    std::vector<GridTrack> expanded;
    if (!ctx.is_auto_fill && !ctx.is_auto_fit) {
        return base_tracks;
    }

    int count = compute_auto_repeat_count(ctx, available_size, base_tracks, gap);
    for (int i = 0; i < count; i++) {
        for (const auto& t : base_tracks) {
            expanded.push_back(t);
        }
    }

    if (ctx.is_auto_fit && expanded.size() > base_tracks.size()) {
        auto it = std::remove_if(expanded.begin(), expanded.end(),
            [](const GridTrack& t) { return t.size == 0.0 && !t.is_flexible; });
        expanded.erase(it, expanded.end());
    }

    if (expanded.empty()) {
        GridTrack fallback;
        fallback.size = available_size;
        expanded.push_back(fallback);
    }

    return expanded;
}

// ---- Masonry Layout Variant ----------------------------------------------

struct MasonryItem {
    std::shared_ptr<LayoutNode> node;
    int column = 0;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

static bool is_masonry_layout(const std::shared_ptr<LayoutNode>& container) {
    const auto& style = container->style;
    return style.grid_template_columns.find(L"masonry") != std::wstring::npos
        || style.grid_template_rows.find(L"masonry") != std::wstring::npos;
}

static void layout_masonry_grid(std::shared_ptr<LayoutNode> container,
                                 const std::vector<GridTrack>& col_tracks,
                                 double available_width)
{
    if (col_tracks.empty() || container->children.empty()) return;

    int column_count = (int)col_tracks.size();
    std::vector<double> column_heights(column_count, 0.0);
    std::vector<double> column_x(column_count, 0.0);

    double current_x = container->box_model.padding_left;
    for (int i = 0; i < column_count; i++) {
        column_x[i] = current_x;
        column_heights[i] = container->box_model.padding_top;
        current_x += col_tracks[i].size;
    }

    std::vector<MasonryItem> placed_items;

    for (auto& child : container->children) {
        if (!child) continue;

        int shortest_col = 0;
        for (int i = 1; i < column_count; i++) {
            if (column_heights[i] < column_heights[shortest_col]) {
                shortest_col = i;
            }
        }

        double item_width = col_tracks[shortest_col].size;
        double aspect_ratio = 1.0;
        if (child->content_box.width > 0 && child->content_box.height > 0) {
            aspect_ratio = child->content_box.width / child->content_box.height;
        }
        double item_height = (aspect_ratio > 0) ? item_width / aspect_ratio : item_width;

        child->content_box.x = column_x[shortest_col];
        child->content_box.y = column_heights[shortest_col];
        child->content_box.width = item_width;
        child->content_box.height = item_height;

        MasonryItem mi;
        mi.node = child;
        mi.column = shortest_col;
        mi.x = child->content_box.x;
        mi.y = child->content_box.y;
        mi.width = item_width;
        mi.height = item_height;
        placed_items.push_back(mi);

        column_heights[shortest_col] += item_height;
    }

    double total_height = 0.0;
    for (double h : column_heights) {
        total_height = std::max(total_height, h);
    }
    container->content_box.height = total_height;
}

// ---- Public API -----------------------------------------------------------

bool grid_ext_is_subgrid(const std::wstring& template_val) {
    return is_subgrid(template_val);
}

bool grid_ext_has_subgrid_ancestor() {
    return has_subgrid_ancestor();
}

void grid_ext_push_subgrid_context(std::shared_ptr<LayoutNode> container,
                                    const std::vector<GridTrack>& col_tracks,
                                    const std::vector<GridTrack>& row_tracks)
{
    push_subgrid_context(container, col_tracks, row_tracks);
}

void grid_ext_pop_subgrid_context() {
    pop_subgrid_context();
}

std::vector<GridTrack> grid_ext_expand_tracks(
    const std::vector<GridTrack>& base_tracks,
    const std::wstring& template_val,
    double available_size,
    double gap)
{
    AutoRepeatContext repeat_ctx = parse_auto_repeat(template_val);
    if (repeat_ctx.is_auto_fill || repeat_ctx.is_auto_fit) {
        return expand_auto_repeated_tracks(base_tracks, repeat_ctx, available_size, gap);
    }

    if (is_subgrid(template_val)) {
        return resolve_subgrid_tracks(template_val, base_tracks);
    }

    return base_tracks;
}

bool grid_ext_is_masonry(const std::shared_ptr<LayoutNode>& container) {
    return is_masonry_layout(container);
}

void grid_ext_layout_masonry(std::shared_ptr<LayoutNode> container,
                              const std::vector<GridTrack>& col_tracks,
                              double available_width)
{
    layout_masonry_grid(container, col_tracks, available_width);
}

} // namespace hre::layout
