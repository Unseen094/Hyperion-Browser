#define NOMINMAX
#include <hre/layout/incremental_layout.hpp>
#include <algorithm>
#include <cmath>

namespace hre::layout {

bool dirty_rect::intersects(const dirty_rect& other) const {
    return !(x + width < other.x || other.x + other.width < x ||
             y + height < other.y || other.y + other.height < y);
}

void dirty_rect::merge(const dirty_rect& other) {
    double new_x = std::min(x, other.x);
    double new_y = std::min(y, other.y);
    double right = std::max(x + width, other.x + other.width);
    double bottom = std::max(y + height, other.y + other.height);
    x = new_x;
    y = new_y;
    width = right - new_x;
    height = bottom - new_y;
}

void dirty_rect::expand(double amount) {
    x -= amount;
    y -= amount;
    width += amount * 2;
    height += amount * 2;
}

incremental_layout::incremental_layout() = default;

void incremental_layout::mark_dirty(const LayoutNode* node, layout_change_type change_type) {
    if (!node) return;

    auto& info = m_dirty_map[node];
    info.is_dirty = true;

    if (change_type == layout_change_type::CHILDREN_CHANGED ||
        change_type == layout_change_type::SUBTREE_REMOVED) {
        info.subtree_invalid = true;
    }
}

void incremental_layout::mark_subtree_dirty(const LayoutNode* node) {
    if (!node) return;

    auto& info = m_dirty_map[node];
    info.is_dirty = true;
    info.subtree_invalid = true;

    for (const auto& child : node->children) {
        mark_subtree_dirty(child.get());
    }
}

void incremental_layout::clear_dirty(const LayoutNode* node) {
    if (!node) return;
    m_dirty_map.erase(node);
}

bool incremental_layout::is_dirty(const LayoutNode* node) const {
    auto it = m_dirty_map.find(node);
    return it != m_dirty_map.end() && it->second.is_dirty;
}

bool incremental_layout::has_dirty_subtree(const LayoutNode* node) const {
    auto it = m_dirty_map.find(node);
    return it != m_dirty_map.end() && it->second.subtree_invalid;
}

const std::vector<const LayoutNode*>& incremental_layout::dirty_nodes() const {
    return m_dirty_nodes;
}

const std::vector<dirty_rect>& incremental_layout::dirty_rects() const {
    return m_dirty_rects;
}

void incremental_layout::compute_dirty_rects() {
    m_dirty_rects.clear();

    for (const auto& [node, info] : m_dirty_map) {
        if (info.is_dirty) {
            m_dirty_rects.push_back(info.rect);
        }
    }

    std::vector<dirty_rect> merged;
    for (const auto& rect : m_dirty_rects) {
        bool found = false;
        for (auto& existing : merged) {
            if (existing.intersects(rect)) {
                existing.merge(rect);
                found = true;
                break;
            }
        }
        if (!found) {
            merged.push_back(rect);
        }
    }

    m_dirty_rects = std::move(merged);
}

void incremental_layout::clear_all_dirty() {
    m_dirty_map.clear();
    m_dirty_nodes.clear();
    m_dirty_rects.clear();
}

void incremental_layout::invalidate_rect(const LayoutNode* node, const dirty_rect& rect) {
    if (!node) return;

    auto& info = m_dirty_map[node];
    info.is_dirty = true;
    info.rect = rect;

    m_dirty_nodes.push_back(node);
}

const dirty_rect& incremental_layout::get_dirty_rect(const LayoutNode* node) const {
    static dirty_rect empty;
    auto it = m_dirty_map.find(node);
    return it != m_dirty_map.end() ? it->second.rect : empty;
}

void incremental_layout::begin_frame() {
    m_accumulated_rects.clear();
}

void incremental_layout::end_frame() {
    compute_dirty_rects();

    for (const auto& rect : m_accumulated_rects) {
        if (m_dirty_callback) {
        }
    }
}

} // namespace hre::layout