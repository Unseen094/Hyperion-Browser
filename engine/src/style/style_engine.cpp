#include <hre/style/style_engine.hpp>

namespace hre::style {

std::map<dom::node*, computed_style> g_computed_styles;

void style_engine::apply_styles(dom::document* doc, const css::stylesheet& ss) {
    g_computed_styles.clear();
    style_recursive(doc, ss);
}

void style_engine::style_recursive(dom::node* node, const css::stylesheet& ss) {
    if (node->type() != dom::node_type::element) {
        for (const auto& child : node->children()) {
            style_recursive(child.get(), ss);
        }
        return;
    }

    auto* el = static_cast<dom::element*>(node);
    computed_style style;

    for (const auto& rule : ss.rules) {
        bool matched = false;
        for (const auto& sel : rule.selectors) {
            if (matches_selector(el, sel)) {
                matched = true;
                break;
            }
        }

        if (matched) {
            for (const auto& decl : rule.declarations) {
                if (decl.name == L"color") style.color = decl.value.data;
                else if (decl.name == L"background-color") style.background_color = decl.value.data;
                else if (decl.name == L"width") style.width = decl.value.number;
                else if (decl.name == L"height") style.height = decl.value.number;
                else if (decl.name == L"padding") {
                    style.padding_top = style.padding_right = style.padding_bottom = style.padding_left = decl.value.number;
                }
                else if (decl.name == L"margin") {
                    style.margin_top = style.margin_right = style.margin_bottom = style.margin_left = decl.value.number;
                }
                else if (decl.name == L"border-width") style.border_width = decl.value.number;
                else if (decl.name == L"border-color") style.border_color = decl.value.data;
                else if (decl.name == L"display") style.display = decl.value.data;
                else if (decl.name == L"flex-direction") style.flex_direction = decl.value.data;
                else if (decl.name == L"justify-content") style.justify_content = decl.value.data;
                else if (decl.name == L"align-items") style.align_items = decl.value.data;
                else if (decl.name == L"flex-grow") style.flex_grow = decl.value.number;
            }
        }
    }
    g_computed_styles[node] = style;

    for (const auto& child : node->children()) {
        style_recursive(child.get(), ss);
    }
}

bool style_engine::matches_selector(dom::element* el, const css::selector& sel) {
    if (!sel.tag_name.empty() && sel.tag_name != el->tag_name()) return false;
    if (!sel.class_name.empty()) {
        const auto& classes = el->class_list();
        bool found = false;
        for (const auto& cls : classes) {
            if (cls == sel.class_name) { found = true; break; }
        }
        if (!found) return false;
    }
    if (!sel.id.empty() && sel.id != el->id()) return false;
    return true;
}

} // namespace hre::style
