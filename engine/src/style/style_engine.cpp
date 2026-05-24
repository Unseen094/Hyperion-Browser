#include <hre/style/style_engine.hpp>
#include <sstream>
#include <algorithm>

namespace hre::style {

std::map<dom::node*, computed_style> g_computed_styles;

static std::wstring trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::pair<std::wstring, std::wstring>> parse_declarations(const std::wstring& block) {
    std::vector<std::pair<std::wstring, std::wstring>> decls;
    size_t pos = 0;
    while (pos < block.size()) {
        size_t colon = block.find(L':', pos);
        if (colon == std::wstring::npos) break;
        std::wstring prop = trim(block.substr(pos, colon - pos));
        size_t semi = block.find(L';', colon + 1);
        if (semi == std::wstring::npos) {
            std::wstring val = trim(block.substr(colon + 1));
            if (!prop.empty()) decls.emplace_back(prop, val);
            break;
        }
        std::wstring val = trim(block.substr(colon + 1, semi - colon - 1));
        if (!prop.empty()) decls.emplace_back(prop, val);
        pos = semi + 1;
    }
    return decls;
}

void style_engine::apply_styles(dom::document* doc, const css::stylesheet& ss) {
    g_computed_styles.clear();
    style_recursive(doc, ss);
}

void style_engine::style_recursive(dom::node* node, const css::stylesheet& ss) {
    if (node->type() == dom::node_type::element) {
        auto* el = static_cast<dom::element*>(node);
        computed_style style;

        for (const auto& raw_rule : ss.raw_rules) {
            size_t brace_open = raw_rule.find(L'{');
            if (brace_open == std::wstring::npos) continue;
            size_t brace_close = raw_rule.find(L'}', brace_open);
            if (brace_close == std::wstring::npos) continue;

            std::wstring selectors_str = trim(raw_rule.substr(0, brace_open));
            std::wstring block = raw_rule.substr(brace_open + 1, brace_close - brace_open - 1);

            std::wistringstream sel_stream(selectors_str);
            std::wstring sel;
            while (std::getline(sel_stream, sel, L',')) {
                sel = trim(sel);
                if (matches_selector(el, sel)) {
                    auto decls = parse_declarations(block);
                    for (auto& [prop, val] : decls) {
                        if (prop == L"color") style.color = val;
                        else if (prop == L"background-color") style.background_color = val;
                        else if (prop == L"background") style.background_color = val;
                        else if (prop == L"display") style.display = val;
                        else if (prop == L"visibility") style.visibility = val;
                        else if (prop == L"font-family") style.font_family = val;
                        else if (prop == L"font-size") style.font_size = std::stof(val);
                        else if (prop == L"font-weight") style.font_weight = val;
                        else if (prop == L"text-align") style.text_align = val;
                        else if (prop == L"border-color") style.border_color = val;
                        else if (prop == L"border-radius") style.border_radius = std::stof(val);
                        else if (prop == L"border-width") style.border_top = style.border_right = style.border_bottom = style.border_left = std::stof(val);
                        else if (prop == L"margin") style.margin_top = style.margin_right = style.margin_bottom = style.margin_left = std::stof(val);
                        else if (prop == L"padding") style.padding_top = style.padding_right = style.padding_bottom = style.padding_left = std::stof(val);
                        else if (prop == L"box-shadow") {
                            std::wistringstream ss_val(val);
                            std::wstring token;
                            std::vector<float> nums;
                            while (ss_val >> token) {
                                if (token.find_first_not_of(L"0123456789.-") == std::wstring::npos) {
                                    nums.push_back(std::stof(token));
                                } else if (token.find(L"#") == 0 || token.find(L"rgb") == 0) {
                                    style.box_shadow_color = token;
                                }
                            }
                            if (nums.size() >= 1) style.box_shadow_offset_x = nums[0];
                            if (nums.size() >= 2) style.box_shadow_offset_y = nums[1];
                            if (nums.size() >= 3) style.box_shadow_blur = nums[2];
                        }
                        else if (prop == L"opacity") style.opacity = std::stof(val);
                        else if (prop == L"flex-direction") style.flex_direction = val;
                        else if (prop == L"justify-content") style.justify_content = val;
                        else if (prop == L"align-items") style.align_items = val;
                        else if (prop == L"flex-grow") style.flex_grow = std::stof(val);
                        else if (prop == L"flex-shrink") style.flex_shrink = std::stof(val);
                        else if (prop == L"position") style.position = val;
                        else if (prop == L"overflow") style.overflow = val;
                    }
                }
            }
        }

        g_computed_styles[node] = style;
    }

    for (const auto& child : node->children()) {
        style_recursive(child.get(), ss);
    }
}

bool style_engine::matches_selector(dom::element* el, const css::selector& sel) {
    if (sel.empty()) return false;
    if (sel == L"*") return true;

    if (sel[0] == L'.') {
        std::wstring cls = sel.substr(1);
        const auto& classes = el->class_list();
        return std::find(classes.begin(), classes.end(), cls) != classes.end();
    }

    if (sel[0] == L'#') {
        std::wstring id = sel.substr(1);
        return el->id() == id;
    }

    if (sel.find(L'.') != std::wstring::npos) {
        size_t dot = sel.find(L'.');
        std::wstring tag = sel.substr(0, dot);
        std::wstring cls = sel.substr(dot + 1);
        if (!tag.empty() && tag != el->tag_name()) return false;
        const auto& classes = el->class_list();
        return std::find(classes.begin(), classes.end(), cls) != classes.end();
    }

    return el->tag_name() == sel;
}

} // namespace hre::style
