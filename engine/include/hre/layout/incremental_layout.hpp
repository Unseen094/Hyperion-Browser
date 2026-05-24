#pragma once

#include <hre/layout/layout_engine.hpp>
#include <string>
#include <map>
#include <vector>
#include <functional>

namespace hre::layout {

enum class layout_change_type {
    STYLE_CHANGED,
    GEOMETRY_CHANGED,
    CHILDREN_CHANGED,
    SUBTREE_REMOVED
};

struct layout_change {
    const LayoutNode* node = nullptr;
    layout_change_type type;
    bool is_dirty = true;
    bool subtree_invalid = false;
};

struct dirty_rect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;

    bool intersects(const dirty_rect& other) const;
    void merge(const dirty_rect& other);
    void expand(double amount);
};

class incremental_layout {
public:
    incremental_layout();

    void mark_dirty(const LayoutNode* node, layout_change_type change_type);
    void mark_subtree_dirty(const LayoutNode* node);
    void clear_dirty(const LayoutNode* node);

    bool is_dirty(const LayoutNode* node) const;
    bool has_dirty_subtree(const LayoutNode* node) const;

    const std::vector<const LayoutNode*>& dirty_nodes() const;
    const std::vector<dirty_rect>& dirty_rects() const;

    void compute_dirty_rects();
    void clear_all_dirty();

    using dirty_callback = std::function<void(const LayoutNode*, const dirty_rect&)>;
    void set_dirty_callback(dirty_callback cb) { m_dirty_callback = cb; }

    void invalidate_rect(const LayoutNode* node, const dirty_rect& rect);
    const dirty_rect& get_dirty_rect(const LayoutNode* node) const;

    void begin_frame();
    void end_frame();

private:
    struct node_dirty_info {
        bool is_dirty = false;
        bool subtree_invalid = false;
        dirty_rect rect;
    };

    std::map<const LayoutNode*, node_dirty_info> m_dirty_map;
    std::vector<const LayoutNode*> m_dirty_nodes;
    std::vector<dirty_rect> m_dirty_rects;
    std::vector<dirty_rect> m_accumulated_rects;

    dirty_callback m_dirty_callback;
};

} // namespace hre::layout