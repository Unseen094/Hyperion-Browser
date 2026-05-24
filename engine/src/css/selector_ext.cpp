#include "hre/css/selector_ext.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <unordered_set>

namespace hre::css {

static std::wstring trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    size_t end = s.find_last_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    return s.substr(start, end - start + 1);
}

static std::vector<std::wstring> split_comma_list(const std::wstring& list) {
    std::vector<std::wstring> result;
    std::wstring current;
    int depth = 0;
    for (wchar_t c : list) {
        if (c == L'(') depth++;
        else if (c == L')') depth--;
        if (c == L',' && depth == 0) {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) result.push_back(trim(current));
    return result;
}

AnPlusB SelectorExtensions::parse_an_plus_b(const std::wstring& expr) {
    AnPlusB result;
    std::wstring s = trim(expr);
    if (s.empty()) return result;

    if (s == L"odd") {
        result.a = 2; result.b = 1; result.valid = true;
        return result;
    }
    if (s == L"even") {
        result.a = 2; result.b = 0; result.valid = true;
        return result;
    }

    bool negative_a = false;
    bool negative_b = false;
    size_t pos = 0;

    if (pos < s.size() && s[pos] == L'-') {
        negative_a = true;
        pos++;
    } else if (pos < s.size() && s[pos] == L'+') {
        pos++;
    }

    std::wstring a_str;
    while (pos < s.size() && std::iswdigit(s[pos])) {
        a_str += s[pos++];
    }

    if (pos < s.size() && (s[pos] == L'n' || s[pos] == L'N')) {
        pos++;
        if (!a_str.empty()) {
            result.a = std::stoi(a_str);
        } else {
            result.a = 1;
        }
        if (negative_a) result.a = -result.a;

        while (pos < s.size() && (s[pos] == L' ' || s[pos] == L'+')) {
            if (s[pos] == L'+') { pos++; break; }
            pos++;
        }

        if (pos < s.size() && s[pos] == L'-') {
            negative_b = true;
            pos++;
        } else if (pos < s.size() && s[pos] == L'+') {
            pos++;
        }

        std::wstring b_str;
        while (pos < s.size() && std::iswdigit(s[pos])) {
            b_str += s[pos++];
        }
        if (!b_str.empty()) {
            result.b = std::stoi(b_str);
            if (negative_b) result.b = -result.b;
        } else if (negative_b) {
            result.b = 0;
        }
        result.valid = true;
    } else {
        result.a = 0;
        if (!a_str.empty()) {
            result.b = std::stoi(a_str);
            if (negative_a) result.b = -result.b;
        } else {
            result.b = 0;
        }
        result.valid = true;
    }

    return result;
}

static bool node_has_class(const dom::node* node, const std::wstring& cls) {
    const auto* elem = dynamic_cast<const dom::element*>(node);
    if (!elem) return false;
    for (const auto& c : elem->class_list()) {
        if (c == cls) return true;
    }
    return false;
}

static bool matches_tag(const dom::node* node, const std::wstring& tag) {
    const auto* elem = dynamic_cast<const dom::element*>(node);
    if (!elem) return false;
    if (tag == L"*") return true;
    return elem->tag_name() == tag;
}

// ---- Attribute Selectors (Task 16-17) -----------------------------------

bool SelectorExtensions::matches_attribute(const dom::node* node, const std::wstring& attr_selector) {
    const auto* elem = dynamic_cast<const dom::element*>(node);
    if (!elem) return false;

    std::wstring s = trim(attr_selector);

    // [attr]
    if (s.size() >= 2 && s[0] == L'[' && s.back() == L']' && s.find(L'=') == std::wstring::npos) {
        std::wstring attr_name = trim(s.substr(1, s.size() - 2));
        return elem->has_attribute(attr_name);
    }

    // [attr=val], [attr~=val], [attr|=val], [attr^=val], [attr$=val], [attr*=val]
    size_t eq_pos = s.find(L'=');
    if (eq_pos == std::wstring::npos) return false;

    wchar_t prefix = eq_pos > 0 ? s[eq_pos - 1] : 0;
    bool has_op_prefix = (prefix == L'~' || prefix == L'|' || prefix == L'^' || prefix == L'$' || prefix == L'*');

    std::wstring attr_name;
    std::wstring attr_value;
    size_t val_start;

    if (has_op_prefix) {
        attr_name = trim(s.substr(1, eq_pos - 2));
        val_start = eq_pos + 1;
    } else {
        attr_name = trim(s.substr(1, eq_pos - 1));
        val_start = eq_pos + 1;
    }

    // Extract value (may be quoted)
    std::wstring val_str = trim(s.substr(val_start));
    if (!val_str.empty() && (val_str.front() == L'"' || val_str.front() == L'\'')) {
        val_str = val_str.substr(1);
    }
    if (!val_str.empty() && (val_str.back() == L'"' || val_str.back() == L'\'' || val_str.back() == L']')) {
        val_str.pop_back();
    }
    if (!val_str.empty() && val_str.back() == L']') {
        val_str.pop_back();
        val_str = trim(val_str);
    }

    std::wstring actual_value = elem->get_attribute(attr_name);

    if (!has_op_prefix) {
        // [attr=val] - exact match
        return actual_value == val_str;
    }

    switch (prefix) {
        case L'~': {
            // [attr~=val] - whitespace-separated list
            std::wistringstream ss(actual_value);
            std::wstring token;
            while (ss >> token) {
                if (token == val_str) return true;
            }
            return false;
        }
        case L'|': {
            // [attr|=val] - exact or followed by hyphen
            return actual_value == val_str ||
                   (actual_value.size() > val_str.size() &&
                    actual_value.substr(0, val_str.size()) == val_str &&
                    actual_value[val_str.size()] == L'-');
        }
        case L'^': {
            // [attr^=val] - starts-with
            return actual_value.size() >= val_str.size() &&
                   actual_value.substr(0, val_str.size()) == val_str;
        }
        case L'$': {
            // [attr$=val] - ends-with
            return actual_value.size() >= val_str.size() &&
                   actual_value.substr(actual_value.size() - val_str.size()) == val_str;
        }
        case L'*': {
            // [attr*=val] - contains substring
            return actual_value.find(val_str) != std::wstring::npos;
        }
    }

    return false;
}

// ---- Pseudo-element recognition (Task 12) --------------------------------

bool SelectorExtensions::is_pseudo_element(const std::wstring& selector, std::wstring& element_part) {
    size_t dbl_colon = selector.find(L"::");
    if (dbl_colon == std::wstring::npos) return false;

    element_part = trim(selector.substr(0, dbl_colon));
    std::wstring pseudo = trim(selector.substr(dbl_colon + 2));

    // Recognize known pseudo-elements
    static const std::unordered_set<std::wstring> known_pseudo_elements = {
        L"before", L"after", L"first-line", L"first-letter",
        L"selection", L"placeholder", L"marker", L"backdrop"
    };

    return known_pseudo_elements.count(pseudo) > 0;
}

// ---- Simple selector ----------------------------------------------------

bool SelectorExtensions::matches_simple_selector(const dom::node* node, const std::wstring& selector) {
    const auto* elem = dynamic_cast<const dom::element*>(node);
    if (!elem) return false;

    std::wstring s = trim(selector);
    if (s.empty()) return false;

    // Handle pseudo-elements: strip and match element part
    std::wstring element_part;
    if (is_pseudo_element(s, element_part)) {
        if (element_part.empty()) return true; // ::before alone matches any
        s = element_part;
    }

    // Strip pseudo-classes for simple matching
    std::wstring clean = s;
    size_t pos = 0;
    while (pos < clean.size()) {
        if (clean[pos] == L':') {
            size_t end = pos;
            while (end < clean.size() && clean[end] != L'(' && clean[end] != L' ' && clean[end] != L'>' && clean[end] != L'+' && clean[end] != L'~') end++;
            if (end < clean.size() && clean[end] == L'(') {
                // Skip functional pseudo-class
                int depth = 1;
                end++;
                while (end < clean.size() && depth > 0) {
                    if (clean[end] == L'(') depth++;
                    if (clean[end] == L')') depth--;
                    end++;
                }
            }
            clean.erase(pos, end - pos);
        } else {
            pos++;
        }
    }
    clean = trim(clean);
    if (clean.empty()) return true;

    // Check attribute selectors embedded in compound selector
    // For simplicity, extract and check the base type selector first
    std::wstring base = clean;
    // Remove attribute selectors [..] for base type matching
    size_t bracket_pos = base.find(L'[');
    if (bracket_pos != std::wstring::npos) {
        base = base.substr(0, bracket_pos);
        base = trim(base);
    }

    // Match the base type/class/id
    if (!base.empty()) {
        if (base[0] == L'.') {
            if (!node_has_class(node, base.substr(1))) return false;
        } else if (base[0] == L'#') {
            if (elem->id() != base.substr(1)) return false;
        } else if (base[0] != L'*' && !matches_tag(node, base)) {
            return false;
        }
    }

    // Check attribute selectors
    bracket_pos = s.find(L'[');
    while (bracket_pos != std::wstring::npos) {
        size_t bracket_end = s.find(L']', bracket_pos);
        if (bracket_end == std::wstring::npos) break;
        std::wstring attr_sel = s.substr(bracket_pos, bracket_end - bracket_pos + 1);
        if (!matches_attribute(node, attr_sel)) return false;
        bracket_pos = s.find(L'[', bracket_end + 1);
    }

    return true;
}

// ---- Attribute selector pseudo-class: :nth-of-type (Task 13) ------------

SelectorMatch SelectorExtensions::matches_nth_of_type(const dom::node* node, const std::wstring& nth_expr) {
    SelectorMatch result;
    if (!node) return result;

    const auto* elem = dynamic_cast<const dom::element*>(node);
    if (!elem) return result;

    const dom::node* parent = node->parent();
    if (!parent) return result;

    AnPlusB expr = parse_an_plus_b(nth_expr);
    if (!expr.valid) return result;

    std::wstring tag = elem->tag_name();
    int index = 1;
    for (const auto& sibling : parent->children()) {
        if (sibling.get() == node) break;
        if (const auto* sib_elem = dynamic_cast<const dom::element*>(sibling.get())) {
            if (sib_elem->tag_name() == tag) index++;
        }
    }

    if (expr.a == 0) {
        result.matches = (index == expr.b);
    } else {
        int adjusted = index - expr.b;
        if (expr.a > 0) {
            result.matches = (adjusted >= 0 && adjusted % expr.a == 0);
        } else if (expr.a < 0) {
            result.matches = (adjusted <= 0 && adjusted % expr.a == 0);
        }
    }

    result.specificity.b++;
    return result;
}

// ---- :lang() pseudo-class (Task 20) -------------------------------------

SelectorMatch SelectorExtensions::matches_lang(const dom::node* node, const std::wstring& lang) {
    SelectorMatch result;
    const auto* elem = dynamic_cast<const dom::element*>(node);
    if (!elem) return result;

    // Check for lang attribute on this element
    std::wstring el_lang = elem->get_attribute(L"lang");
    if (el_lang.empty()) {
        // Walk up to find inherited lang
        const dom::node* current = node->parent();
        while (current) {
            if (const auto* parent_elem = dynamic_cast<const dom::element*>(current)) {
                el_lang = parent_elem->get_attribute(L"lang");
                if (!el_lang.empty()) break;
            }
            current = current->parent();
        }
    }

    if (el_lang.empty()) return result;

    // Trim to primary language subtag for matching
    size_t dash = el_lang.find(L'-');
    std::wstring primary = (dash != std::wstring::npos) ? el_lang.substr(0, dash) : el_lang;

    // Check for prefix match: lang="en" matches :lang(en), :lang(en-US)
    // Also check if el_lang starts with target lang followed by '-'
    result.matches = (primary == lang) ||
                     (el_lang.size() > lang.size() &&
                      el_lang.substr(0, lang.size()) == lang &&
                      el_lang[lang.size()] == L'-');

    result.specificity.b++;
    return result;
}

// ---- Existing selectors -------------------------------------------------

SelectorMatch SelectorExtensions::matches_has(const dom::node* node, const std::wstring& relative_selector) {
    SelectorMatch result;
    if (!node) return result;

    std::wstring rs = trim(relative_selector);
    std::vector<const dom::node*> candidates = precompute_potential_has_matches(node, rs);

    for (const auto* descendant : candidates) {
        if (descendant == node) continue;
        if (matches_simple_selector(descendant, rs)) {
            result.matches = true;
            break;
        }
    }

    result.specificity = specificity_of(rs);
    result.specificity.b++;
    return result;
}

SelectorMatch SelectorExtensions::matches_nth_child(const dom::node* node, const std::wstring& nth_expr) {
    SelectorMatch result;
    if (!node) return result;

    const dom::node* parent = node->parent();
    if (!parent) return result;

    AnPlusB expr = parse_an_plus_b(nth_expr);
    if (!expr.valid) return result;

    int index = 1;
    for (const auto& sibling : parent->children()) {
        if (sibling.get() == node) break;
        if (dynamic_cast<const dom::element*>(sibling.get())) {
            index++;
        }
    }

    if (expr.a == 0) {
        result.matches = (index == expr.b);
    } else {
        int adjusted = index - expr.b;
        if (expr.a > 0) {
            result.matches = (adjusted >= 0 && adjusted % expr.a == 0);
        } else if (expr.a < 0) {
            result.matches = (adjusted <= 0 && adjusted % expr.a == 0);
        }
    }

    result.specificity.b++;
    return result;
}

SelectorMatch SelectorExtensions::matches_is(const dom::node* node, const std::vector<std::wstring>& selector_list) {
    SelectorMatch result;
    Specificity max_spec;
    for (const auto& sel : selector_list) {
        Specificity spec = specificity_of(sel);
        if (spec > max_spec) max_spec = spec;

        if (matches_simple_selector(node, sel)) {
            result.matches = true;
        }
    }
    result.specificity = max_spec;
    return result;
}

SelectorMatch SelectorExtensions::matches_where(const dom::node* node, const std::vector<std::wstring>& selector_list) {
    SelectorMatch result;
    for (const auto& sel : selector_list) {
        if (matches_simple_selector(node, sel)) {
            result.matches = true;
        }
    }
    return result;
}

SelectorMatch SelectorExtensions::matches_not(const dom::node* node, const std::vector<std::wstring>& selector_list) {
    SelectorMatch result;
    Specificity max_spec;
    for (const auto& sel : selector_list) {
        Specificity spec = specificity_of(sel);
        if (spec > max_spec) max_spec = spec;
        if (matches_simple_selector(node, sel)) {
            result.matches = false;
            result.specificity = max_spec;
            return result;
        }
    }
    result.matches = true;
    result.specificity = max_spec;
    return result;
}

bool SelectorExtensions::matches_descendant(const dom::node* node, const std::wstring& ancestor_selector) {
    if (!node) return false;
    const dom::node* current = node->parent();
    while (current) {
        if (matches_simple_selector(current, ancestor_selector)) return true;
        current = current->parent();
    }
    return false;
}

bool SelectorExtensions::matches_child(const dom::node* node, const std::wstring& parent_selector) {
    if (!node) return false;
    const dom::node* parent = node->parent();
    if (!parent) return false;
    return matches_simple_selector(parent, parent_selector);
}

bool SelectorExtensions::matches_next_sibling(const dom::node* node, const std::wstring& prev_selector) {
    if (!node) return false;
    const dom::node* parent = node->parent();
    if (!parent) return false;

    for (const auto& sibling : parent->children()) {
        if (sibling.get() == node) break;
        if (dynamic_cast<const dom::element*>(sibling.get())) {
            if (matches_simple_selector(sibling.get(), prev_selector)) {
                return true;
            }
        }
    }
    return false;
}

bool SelectorExtensions::matches_subsequent_sibling(const dom::node* node, const std::wstring& prev_selector) {
    if (!node) return false;
    const dom::node* parent = node->parent();
    if (!parent) return false;

    for (const auto& sibling : parent->children()) {
        if (sibling.get() == node) break;
        if (dynamic_cast<const dom::element*>(sibling.get())) {
            if (matches_simple_selector(sibling.get(), prev_selector)) {
                return true;
            }
        }
    }
    return false;
}

// ---- Specificity (Task 19) -----------------------------------------------

Specificity SelectorExtensions::specificity_of(const std::wstring& selector) {
    Specificity spec;
    std::wstring s = trim(selector);

    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'#') {
            spec.a++;
        } else if (s[i] == L'.' || s[i] == L':') {
            if (s[i] == L':' && i + 1 < s.size() && s[i + 1] == L':') {
                // ::pseudo-element → type-level specificity
                spec.c++;
                i++;
            } else {
                spec.b++;
            }
        } else if (s[i] == L'[') {
            spec.b++;
        } else if (std::iswalnum(s[i])) {
            spec.c++;
        }
    }

    return spec;
}

// ---- Precompute for :has() ----------------------------------------------

std::vector<const dom::node*> SelectorExtensions::precompute_potential_has_matches(
    const dom::node* root,
    const std::wstring& relative_selector)
{
    std::vector<const dom::node*> matches;
    if (!root) return matches;

    std::function<void(const dom::node*)> traverse = [&](const dom::node* n) {
        if (n != root && matches_simple_selector(n, relative_selector)) {
            matches.push_back(n);
        }
        for (const auto& child : n->children()) {
            traverse(child.get());
        }
    };

    traverse(root);
    return matches;
}

} // namespace hre::css
