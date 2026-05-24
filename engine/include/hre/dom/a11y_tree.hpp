#pragma once

#include "hre/dom/node.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <functional>
#include <cstdint>

namespace hre::dom {

enum class a11y_role {
    NONE,
    BUTTON,
    LINK,
    HEADING,
    TEXT_BOX,
    CHECKBOX,
    RADIO,
    COMBOBOX,
    LISTBOX,
    SLIDER,
    PROGRESSBAR,
    TAB,
    TAB_LIST,
    TREE,
    TREE_ITEM,
    MENU,
    MENU_ITEM,
    DIALOG,
    ALERT,
    TOOLTIP,
    NAVIGATION,
    BANNER,
    MAIN,
    COMPLEMENTARY,
    CONTENT_INFO,
    FORM,
    SEARCH,
    REGION,
    IMG,
    AUDIO,
    VIDEO,
    TABLE,
    GRID,
    ROW,
    CELL,
    COLUMN_HEADER,
    ROW_HEADER,
    LIST,
    LIST_ITEM,
    PRESENTATION,
    APPLICATION,
    DOCUMENT,
    MATH,
    STATUS,
    TIMER,
    LOG
};

enum class a11y_state {
    NORMAL,
    FOCUSED,
    CHECKED,
    SELECTED,
    DISABLED,
    EXPANDED,
    COLLAPSED,
    BUSY,
    INVALID,
    REQUIRED,
    READ_ONLY
};

struct a11y_attributes {
    std::wstring role_str;
    a11y_role role = a11y_role::NONE;

    std::wstring label;
    std::wstring labelled_by;
    std::wstring described_by;

    bool hidden = false;
    bool disabled = false;
    bool readonly = false;
    bool required = false;
    bool selected = false;
    bool expanded = false;
    bool pressed = false;

    int level = 0;
    int tab_index = -1;

    std::wstring placeholder;
    std::wstring value_text;
    std::wstring key_shortcuts;

    int64_t pos_in_set = 0;
    int64_t set_size = 0;
};

struct a11y_bounds {
    float x = 0, y = 0, width = 0, height = 0;
};

struct a11y_node {
    a11y_role role = a11y_role::NONE;
    a11y_attributes attributes;
    a11y_bounds bounds;
    std::wstring computed_name;
    std::wstring computed_description;
    std::vector<a11y_state> states;
    node* dom_node = nullptr;
    std::vector<std::shared_ptr<a11y_node>> children;
    std::weak_ptr<a11y_node> parent;

    bool focusable() const {
        return attributes.tab_index >= 0 || role == a11y_role::BUTTON ||
               role == a11y_role::LINK || role == a11y_role::TEXT_BOX;
    }
};

class a11y_tree {
public:
    a11y_tree() = default;
    ~a11y_tree() = default;

    void build(node* root);
    void rebuild(node* root);
    void clear();

    std::shared_ptr<a11y_node> root() const { return root_; }
    bool empty() const { return root_ == nullptr; }

    std::shared_ptr<a11y_node> hit_test(float x, float y) const;
    std::shared_ptr<a11y_node> find_by_dom_node(node* dom_node) const;
    std::shared_ptr<a11y_node> find_focused() const;

    std::vector<std::shared_ptr<a11y_node>> get_all_nodes() const;

    // Name computation (accName algorithm)
    std::wstring compute_name(const std::shared_ptr<a11y_node>& a11y_node) const;
    std::wstring compute_description(const std::shared_ptr<a11y_node>& a11y_node) const;

    // Event handling
    void update_node(node* dom_node);

    a11y_role aria_role_to_enum(const std::wstring& role_str) const;
    std::wstring role_to_string(a11y_role role) const;

private:
    std::shared_ptr<a11y_node> build_recursive(node* dom_node,
                                                std::shared_ptr<a11y_node> parent);
    void parse_aria_attributes(node* dom_node, a11y_attributes& attrs) const;
    a11y_role compute_implicit_role(node* dom_node) const;
    std::wstring compute_name_from_content(const std::shared_ptr<a11y_node>& a11y_node) const;
    std::wstring compute_name_from_attribute(const a11y_attributes& attrs) const;
    std::wstring compute_name_from_labelledby(const a11y_attributes& attrs) const;

    std::shared_ptr<a11y_node> root_;
    mutable std::map<node*, std::weak_ptr<a11y_node>> node_map_;
};

} // namespace hre::dom
