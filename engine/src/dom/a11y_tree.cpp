#include <hre/dom/a11y_tree.hpp>
#include <algorithm>
#include <cctype>
#include <queue>
#include <sstream>

namespace hre::dom {

// ---- ARIA Role Mapping -----------------------------------------------------

a11y_role a11y_tree::aria_role_to_enum(const std::wstring& role_str) const {
    std::wstring r = role_str;
    for (auto& c : r) c = static_cast<wchar_t>(std::tolower(c));

    if (r == L"button") return a11y_role::BUTTON;
    if (r == L"link") return a11y_role::LINK;
    if (r == L"heading") return a11y_role::HEADING;
    if (r == L"textbox") return a11y_role::TEXT_BOX;
    if (r == L"checkbox") return a11y_role::CHECKBOX;
    if (r == L"radio") return a11y_role::RADIO;
    if (r == L"combobox") return a11y_role::COMBOBOX;
    if (r == L"listbox") return a11y_role::LISTBOX;
    if (r == L"slider") return a11y_role::SLIDER;
    if (r == L"progressbar") return a11y_role::PROGRESSBAR;
    if (r == L"tab") return a11y_role::TAB;
    if (r == L"tablist") return a11y_role::TAB_LIST;
    if (r == L"tree") return a11y_role::TREE;
    if (r == L"treeitem") return a11y_role::TREE_ITEM;
    if (r == L"menu") return a11y_role::MENU;
    if (r == L"menuitem") return a11y_role::MENU_ITEM;
    if (r == L"dialog") return a11y_role::DIALOG;
    if (r == L"alert") return a11y_role::ALERT;
    if (r == L"tooltip") return a11y_role::TOOLTIP;
    if (r == L"navigation") return a11y_role::NAVIGATION;
    if (r == L"banner") return a11y_role::BANNER;
    if (r == L"main") return a11y_role::MAIN;
    if (r == L"complementary") return a11y_role::COMPLEMENTARY;
    if (r == L"contentinfo") return a11y_role::CONTENT_INFO;
    if (r == L"form") return a11y_role::FORM;
    if (r == L"search") return a11y_role::SEARCH;
    if (r == L"region") return a11y_role::REGION;
    if (r == L"img") return a11y_role::IMG;
    if (r == L"audio") return a11y_role::AUDIO;
    if (r == L"video") return a11y_role::VIDEO;
    if (r == L"table") return a11y_role::TABLE;
    if (r == L"grid") return a11y_role::GRID;
    if (r == L"row") return a11y_role::ROW;
    if (r == L"cell") return a11y_role::CELL;
    if (r == L"columnheader") return a11y_role::COLUMN_HEADER;
    if (r == L"rowheader") return a11y_role::ROW_HEADER;
    if (r == L"list") return a11y_role::LIST;
    if (r == L"listitem") return a11y_role::LIST_ITEM;
    if (r == L"presentation") return a11y_role::PRESENTATION;
    if (r == L"application") return a11y_role::APPLICATION;
    if (r == L"document") return a11y_role::DOCUMENT;
    if (r == L"math") return a11y_role::MATH;
    if (r == L"status") return a11y_role::STATUS;
    if (r == L"timer") return a11y_role::TIMER;
    if (r == L"log") return a11y_role::LOG;
    return a11y_role::NONE;
}

std::wstring a11y_tree::role_to_string(a11y_role role) const {
    switch (role) {
        case a11y_role::BUTTON: return L"button";
        case a11y_role::LINK: return L"link";
        case a11y_role::HEADING: return L"heading";
        case a11y_role::TEXT_BOX: return L"textbox";
        case a11y_role::CHECKBOX: return L"checkbox";
        case a11y_role::RADIO: return L"radio";
        case a11y_role::COMBOBOX: return L"combobox";
        case a11y_role::SLIDER: return L"slider";
        default: return L"";
    }
}

a11y_role a11y_tree::compute_implicit_role(node* dom_node) const {
    if (dom_node->type() != node_type::element) return a11y_role::NONE;

    element* el = static_cast<element*>(dom_node);
    std::wstring tag = el->tag_name();
    for (auto& c : tag) c = static_cast<wchar_t>(std::tolower(c));

    if (tag == L"a" || tag == L"area") return a11y_role::LINK;
    if (tag == L"button") return a11y_role::BUTTON;
    if (tag == L"h1" || tag == L"h2" || tag == L"h3" ||
        tag == L"h4" || tag == L"h5" || tag == L"h6") return a11y_role::HEADING;
    if (tag == L"input") {
        std::wstring type = el->get_attribute(L"type");
        if (type == L"checkbox") return a11y_role::CHECKBOX;
        if (type == L"radio") return a11y_role::RADIO;
        if (type == L"text" || type == L"search" || type.empty()) return a11y_role::TEXT_BOX;
        return a11y_role::TEXT_BOX;
    }
    if (tag == L"select") return a11y_role::COMBOBOX;
    if (tag == L"textarea") return a11y_role::TEXT_BOX;
    if (tag == L"img") return a11y_role::IMG;
    if (tag == L"nav") return a11y_role::NAVIGATION;
    if (tag == L"main") return a11y_role::MAIN;
    if (tag == L"header") return a11y_role::BANNER;
    if (tag == L"footer") return a11y_role::CONTENT_INFO;
    if (tag == L"form") return a11y_role::FORM;
    if (tag == L"table") return a11y_role::TABLE;
    if (tag == L"ul" || tag == L"ol") return a11y_role::LIST;
    if (tag == L"li") return a11y_role::LIST_ITEM;
    if (tag == L"dialog") return a11y_role::DIALOG;

    return a11y_role::NONE;
}

// ---- ARIA Attribute Parsing ------------------------------------------------

void a11y_tree::parse_aria_attributes(node* dom_node, a11y_attributes& attrs) const {
    if (dom_node->type() != node_type::element) return;

    element* el = static_cast<element*>(dom_node);

    attrs.role_str = el->get_attribute(L"role");
    attrs.role = aria_role_to_enum(attrs.role_str);
    if (attrs.role == a11y_role::NONE) {
        attrs.role = compute_implicit_role(dom_node);
    }

    attrs.label = el->get_attribute(L"aria-label");
    attrs.labelled_by = el->get_attribute(L"aria-labelledby");
    attrs.described_by = el->get_attribute(L"aria-describedby");

    attrs.hidden = el->get_attribute(L"aria-hidden") == L"true";
    attrs.disabled = el->get_attribute(L"aria-disabled") == L"true";
    attrs.readonly = el->get_attribute(L"aria-readonly") == L"true";
    attrs.required = el->get_attribute(L"aria-required") == L"true";
    attrs.selected = el->get_attribute(L"aria-selected") == L"true";
    attrs.expanded = el->get_attribute(L"aria-expanded") == L"true";

    std::wstring level_str = el->get_attribute(L"aria-level");
    if (!level_str.empty()) attrs.level = std::stoi(level_str);

    std::wstring tabindex_str = el->get_attribute(L"tabindex");
    if (!tabindex_str.empty()) attrs.tab_index = std::stoi(tabindex_str);

    // Infer heading level from tag name
    if (attrs.role == a11y_role::HEADING && attrs.level == 0) {
        std::wstring tag = el->tag_name();
        if (tag.size() == 2 && (tag[0] == L'H' || tag[0] == L'h')) {
            attrs.level = tag[1] - L'0';
        }
    }

    attrs.placeholder = el->get_attribute(L"placeholder");
    attrs.value_text = el->get_attribute(L"value");
    attrs.key_shortcuts = el->get_attribute(L"aria-keyshortcuts");

    std::wstring posinset = el->get_attribute(L"aria-posinset");
    if (!posinset.empty()) attrs.pos_in_set = std::stoll(posinset);

    std::wstring setsize = el->get_attribute(L"aria-setsize");
    if (!setsize.empty()) attrs.set_size = std::stoll(setsize);
}

// ---- Name Computation (accName Algorithm) ----------------------------------

std::wstring a11y_tree::compute_name_from_attribute(const a11y_attributes& attrs) const {
    if (!attrs.label.empty()) return attrs.label;
    return L"";
}

std::wstring a11y_tree::compute_name_from_labelledby(const a11y_attributes& attrs) const {
    if (attrs.labelled_by.empty()) return L"";

    std::vector<std::wstring> ids;
    std::wistringstream stream(attrs.labelled_by);
    std::wstring id;
    while (stream >> id) ids.push_back(id);

    std::wstring result;
    for (const auto& id_str : ids) {
        // Resolve ID reference by walking DOM
        // In a full engine, this would use getElementById
        result += id_str + L" ";
    }

    return result;
}

std::wstring a11y_tree::compute_name_from_content(const std::shared_ptr<a11y_node>& a11y_node) const {
    if (!a11y_node || !a11y_node->dom_node) return L"";

    node* n = a11y_node->dom_node;
    std::wstring name;

    for (const auto& child : n->children()) {
        if (child->type() == node_type::text) {
            text_node* tn = static_cast<text_node*>(child.get());
            name += tn->text();
        } else if (child->type() == node_type::element) {
            auto it = node_map_.find(child.get());
            if (it != node_map_.end()) {
                auto a11y_child = it->second.lock();
                if (a11y_child) {
                    name += compute_name_from_content(a11y_child);
                }
            }
        }
    }

    // Trim
    while (!name.empty() && name[0] == L' ') name.erase(0, 1);
    while (!name.empty() && name.back() == L' ') name.pop_back();

    return name;
}

std::wstring a11y_tree::compute_name(const std::shared_ptr<a11y_node>& a11y_node) const {
    if (!a11y_node) return L"";

    // accName algorithm order:
    // 1. aria-labelledby (if valid reference)
    std::wstring name = compute_name_from_labelledby(a11y_node->attributes);
    if (!name.empty()) return name;

    // 2. aria-label
    name = compute_name_from_attribute(a11y_node->attributes);
    if (!name.empty()) return name;

    // 3. Element-specific fallback
    if (a11y_node->dom_node && a11y_node->dom_node->type() == node_type::element) {
        element* el = static_cast<element*>(a11y_node->dom_node);
        std::wstring tag = el->tag_name();
        for (auto& c : tag) c = static_cast<wchar_t>(std::tolower(c));

        if (tag == L"img") {
            std::wstring alt = el->get_attribute(L"alt");
            if (!alt.empty()) return alt;
        }

        if (tag == L"input") {
            std::wstring placeholder = el->get_attribute(L"placeholder");
            if (!placeholder.empty()) return placeholder;
        }
    }

    // 4. Content (for certain roles)
    if (a11y_node->role == a11y_role::LINK || a11y_node->role == a11y_role::BUTTON ||
        a11y_node->role == a11y_role::HEADING || a11y_node->role == a11y_role::LIST_ITEM) {
        name = compute_name_from_content(a11y_node);
        if (!name.empty()) return name;
    }

    // 5. Title attribute fallback
    if (a11y_node->dom_node && a11y_node->dom_node->type() == node_type::element) {
        element* el = static_cast<element*>(a11y_node->dom_node);
        name = el->get_attribute(L"title");
        if (!name.empty()) return name;
    }

    return L"";
}

std::wstring a11y_tree::compute_description(const std::shared_ptr<a11y_node>& a11y_node) const {
    if (!a11y_node) return L"";

    // 1. aria-describedby
    if (!a11y_node->attributes.described_by.empty()) {
        return a11y_node->attributes.described_by;
    }

    // 2. title attribute (if different from name)
    if (a11y_node->dom_node && a11y_node->dom_node->type() == node_type::element) {
        element* el = static_cast<element*>(a11y_node->dom_node);
        std::wstring title = el->get_attribute(L"title");
        if (!title.empty() && title != a11y_node->computed_name) {
            return title;
        }
    }

    return L"";
}

// ---- Tree Building ---------------------------------------------------------

std::shared_ptr<a11y_node> a11y_tree::build_recursive(node* dom_node,
                                                        std::shared_ptr<a11y_node> parent) {
    if (!dom_node) return nullptr;

    a11y_attributes attrs;
    parse_aria_attributes(dom_node, attrs);

    if (attrs.hidden) return nullptr;

    auto a11y_node = std::make_shared<::hre::dom::a11y_node>();
    a11y_node->attributes = attrs;
    a11y_node->role = attrs.role;
    a11y_node->dom_node = dom_node;
    a11y_node->parent = parent;

    node_map_[dom_node] = a11y_node;

    for (const auto& child : dom_node->children()) {
        auto a11y_child = build_recursive(child.get(), a11y_node);
        if (a11y_child) {
            a11y_node->children.push_back(a11y_child);
        }
    }

    a11y_node->computed_name = compute_name(a11y_node);
    a11y_node->computed_description = compute_description(a11y_node);

    if (a11y_node->focusable()) {
        a11y_node->states.push_back(a11y_state::NORMAL);
    }

    return a11y_node;
}

void a11y_tree::build(node* root) {
    clear();
    if (!root) return;
    root_ = build_recursive(root, nullptr);
}

void a11y_tree::rebuild(node* root) {
    build(root);
}

void a11y_tree::clear() {
    root_.reset();
    node_map_.clear();
}

void a11y_tree::update_node(node* dom_node) {
    auto it = node_map_.find(dom_node);
    if (it != node_map_.end()) {
        auto a11y_node = it->second.lock();
        if (a11y_node) {
            a11y_attributes new_attrs;
            parse_aria_attributes(dom_node, new_attrs);
            a11y_node->attributes = new_attrs;
            a11y_node->computed_name = compute_name(a11y_node);
            a11y_node->computed_description = compute_description(a11y_node);
        }
    }
}

// ---- Hit Testing -----------------------------------------------------------

std::shared_ptr<a11y_node> a11y_tree::hit_test(float x, float y) const {
    if (!root_) return nullptr;

    std::function<std::shared_ptr<a11y_node>(const std::shared_ptr<a11y_node>&)> find =
        [&](const std::shared_ptr<a11y_node>& node) -> std::shared_ptr<a11y_node> {
        if (!node) return nullptr;

        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
            auto result = find(*it);
            if (result) return result;
        }

        if (x >= node->bounds.x && x <= node->bounds.x + node->bounds.width &&
            y >= node->bounds.y && y <= node->bounds.y + node->bounds.height) {
            if (!node->attributes.hidden) return node;
        }

        return nullptr;
    };

    return find(root_);
}

// ---- Find by DOM Node ------------------------------------------------------

std::shared_ptr<a11y_node> a11y_tree::find_by_dom_node(node* dom_node) const {
    auto it = node_map_.find(dom_node);
    if (it != node_map_.end()) {
        return it->second.lock();
    }
    return nullptr;
}

std::shared_ptr<a11y_node> a11y_tree::find_focused() const {
    std::vector<std::shared_ptr<a11y_node>> all = get_all_nodes();
    for (const auto& n : all) {
        for (auto s : n->states) {
            if (s == a11y_state::FOCUSED) return n;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<a11y_node>> a11y_tree::get_all_nodes() const {
    std::vector<std::shared_ptr<a11y_node>> result;

    std::function<void(const std::shared_ptr<a11y_node>&)> traverse =
        [&](const std::shared_ptr<a11y_node>& node) {
        if (!node) return;
        result.push_back(node);
        for (const auto& child : node->children) {
            traverse(child);
        }
    };

    traverse(root_);
    return result;
}

} // namespace hre::dom
