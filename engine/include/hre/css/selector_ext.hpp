#pragma once

#include "hre/css/style_engine.hpp"
#include "hre/dom/node.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace hre::css {

struct AnPlusB {
    int a = 0;
    int b = 0;
    bool valid = false;
};

struct SelectorMatch {
    bool matches = false;
    Specificity specificity;
};

class SelectorExtensions {
public:
    static SelectorMatch matches_has(const dom::Node* node, const std::wstring& relative_selector);
    static SelectorMatch matches_nth_child(const dom::Node* node, const std::wstring& nth_expr);
    static SelectorMatch matches_nth_of_type(const dom::Node* node, const std::wstring& nth_expr);
    static SelectorMatch matches_is(const dom::Node* node, const std::vector<std::wstring>& selector_list);
    static SelectorMatch matches_where(const dom::Node* node, const std::vector<std::wstring>& selector_list);
    static SelectorMatch matches_not(const dom::Node* node, const std::vector<std::wstring>& selector_list);
    static SelectorMatch matches_lang(const dom::Node* node, const std::wstring& lang);

    static AnPlusB parse_an_plus_b(const std::wstring& expr);

    static bool matches_descendant(const dom::Node* node, const std::wstring& ancestor_selector);
    static bool matches_child(const dom::Node* node, const std::wstring& parent_selector);
    static bool matches_next_sibling(const dom::Node* node, const std::wstring& prev_selector);
    static bool matches_subsequent_sibling(const dom::Node* node, const std::wstring& prev_selector);

    // Attribute selectors
    static bool matches_attribute(const dom::Node* node, const std::wstring& attr_selector);

    // Pseudo-element recognition
    static bool is_pseudo_element(const std::wstring& selector, std::wstring& element_part);

    static Specificity specificity_of(const std::wstring& selector);
    static bool matches_simple_selector(const dom::Node* node, const std::wstring& selector);

    static std::vector<const dom::Node*> precompute_potential_has_matches(
        const dom::Node* root,
        const std::wstring& relative_selector);
};

} // namespace hre::css
