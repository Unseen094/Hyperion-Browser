#pragma once

#include "hre/layout/box_model.hpp"
#include <memory>
#include <vector>

namespace hre::layout {
struct LayoutNode;

struct TableCell {
    std::shared_ptr<LayoutNode> node;
    int row = 0, col = 0, rowspan = 1, colspan = 1;
    double x = 0, y = 0, width = 0, height = 0;
    double min_width = 0;
};

struct TableRow {
    std::vector<TableCell> cells;
    double height = 0;
    double y = 0;
};

struct TableSection {
    std::vector<TableRow> rows;
    double y = 0;
    double height = 0;
};

class TableEngine {
public:
    TableEngine() = default;

    void layout_table(
        std::shared_ptr<LayoutNode> table,
        double available_width,
        double available_height
    );

private:
    void collect_cells(std::shared_ptr<LayoutNode> table, std::vector<TableCell>& cells, double available_width);
    void compute_column_widths(std::vector<TableCell>& cells, double total_width);
    double compute_total_width(const std::vector<TableCell>& cells);
    void resolve_anonymous_objects(std::shared_ptr<LayoutNode> table);
    void apply_border_collapse(std::shared_ptr<LayoutNode> table);
    void layout_table_caption(std::shared_ptr<LayoutNode> table, double width);
    void position_table_sections(const std::vector<TableSection>& sections, double table_y);
};

} // namespace hre::layout
