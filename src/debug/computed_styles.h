#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <mutex>

namespace hyperion::debug::css {

// CSS property name enumeration for computation
class ComputedStyleEngine {
public:
    // CSS property-to-value mapping
    struct StyleProperty {
        std::string value;
        bool is_inherited;
        std::string priority; // "important" or ""
        std::string source;   // origin (user-agent, user, author)
    };
    
    // Common CSS properties with default values
    static std::unordered_map<std::string, StyleProperty> default_style_properties() {
        return {
            // Color and background
            {"color", {"#000000", true, "", "author"}},
            {"background-color", {"transparent", false, "", "author"}},
            {"opacity", {"1", false, "", "author"}},
            
            // Font
            {"font-family", {"sans-serif", true, "", "user-agent"}},
            {"font-size", {"16px", true, "", "user-agent"}},
            {"font-weight", {"normal", true, "", "user-agent"}},
            {"line-height", {"normal", true, "", "user-agent"}},
            
            // Box model
            {"display", {"inline", false, "", "author"}},
            {"width", {"auto", false, "", "author"}},
            {"height", {"auto", false, "", "author"}},
            {"margin-top", {"0px", false, "", "author"}},
            {"margin-right", {"0px", false, "", "author"}},
            {"margin-bottom", {"0px", false, "", "author"}},
            {"margin-left", {"0px", false, "", "author"}},
            {"padding-top", {"0px", false, "", "author"}},
            {"padding-right", {"0px", false, "", "author"}},
            {"padding-bottom", {"0px", false, "", "author"}},
            {"padding-left", {"0px", false, "", "author"}},
            {"border-top-width", {"medium", false, "", "author"}},
            {"border-right-width", {"medium", false, "", "author"}},
            {"border-bottom-width", {"medium", false, "", "author"}},
            {"border-left-width", {"medium", false, "", "author"}},
            
            // Position and layout
            {"position", {"static", false, "", "author"}},
            {"top", {"auto", false, "", "author"}},
            {"right", {"auto", false, "", "author"}},
            {"bottom", {"auto", false, "", "author"}},
            {"left", {"auto", false, "", "author"}},
            {"z-index", {"auto", false, "", "author"}},
            
            // Flex and Grid
            {"flex-direction", {"row", false, "", "author"}},
            {"justify-content", {"flex-start", false, "", "author"}},
            {"align-items", {"stretch", false, "", "author"}},
            {"grid-template-columns", {"none", false, "", "author"}},
            
            // Text
            {"text-align", {"left", true, "", "author"}},
            {"text-decoration", {"none", false, "", "user-agent"}},
            {"text-transform", {"none", true, "", "user-agent"}},
            {"letter-spacing", {"normal", true, "", "user-agent"}},
            {"word-spacing", {"normal", true, "", "user-agent"}},
            {"white-space", {"normal", true, "", "user-agent"}},
            
            // Box sizing
            {"box-sizing", {"content-box", false, "", "user-agent"}},
            
            // Overflow
            {"overflow", {"visible", false, "", "author"}},
            {"overflow-x", {"visible", false, "", "author"}},
            {"overflow-y", {"visible", false, "", "author"}},
            
            // Misc
            {"visibility", {"visible", true, "", "user-agent"}},
            {"border-collapse", {"separate", false, "", "user-agent"}},
        };
    }
    
    // Get computed style for element
    std::unordered_map<std::string, StyleProperty> get_computed_style(
        uint32_t element_id,
        const std::unordered_map<std::string, std::string>& inline_styles,
        const std::unordered_map<std::string, std::string>& user_stylesheet = {},
        const std::unordered_map<std::string, std::string>& user_agent_stylesheet = {}) {
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto computed = default_style_properties();
        
        // Apply inline styles with importance
        for (const auto& [property, value] : inline_styles) {
            std::string prop_lower = property;
            std::transform(prop_lower.begin(), prop_lower.end(), prop_lower.begin(), ::tolower);
            
            if (computed.find(prop_lower) != computed.end()) {
                computed[prop_lower].value = value;
                computed[prop_lower].source = "author";
            } else {
                // Add custom property
                computed[prop_lower] = {
                    value, false, "important", "author"
                };
            }
        }
        
        // Apply user stylesheet (lower priority than inline)
        for (const auto& [property, value] : user_stylesheet) {
            std::string prop_lower = property;
            std::transform(prop_lower.begin(), prop_lower.end(), prop_lower.begin(), ::tolower);
            
            if (computed.find(prop_lower) != computed.end()) {
                // User styles override author unless marked !important
                size_t important_pos = value.find("!important");
                if (important_pos != std::string::npos) {
                    computed[prop_lower].value = value.substr(0, important_pos - 1);
                    computed[prop_lower].priority = "important";
                } else {
                    computed[prop_lower].value = value;
                    computed[prop_lower].priority = "";
                }
                computed[prop_lower].source = "user";
            }
        }
        
        // Calculate inheritance and shorthand expansion
        expand_shorthand_properties(computed);
        apply_inheritance(computed, element_id);
        
        return computed;
    }
    
    // Get CSS value for specific property
    std::string get_property_value(const std::unordered_map<std::string, StyleProperty>& computed,
                               const std::string& property) {
        std::string prop_lower = property;
        std::transform(prop_lower.begin(), prop_lower.end(), prop_lower.begin(), ::tolower);
        
        auto it = computed.find(prop_lower);
        if (it != computed.end()) {
            return it->second.value;
        }
        return {};
    }
    
    // Get all inherited properties for element
    std::unordered_map<std::string, std::string> get_inherited_properties(
        const std::unordered_map<std::string, StyleProperty>& computed_styles) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::string, std::string> inherited;
        
        for (const auto& [prop, style] : computed_styles) {
            if (style.is_inherited) {
                inherited[prop] = style.value;
            }
        }
        
        return inherited;
    }
    
    // Get computed styles as CSS string (only different from defaults)
    std::string get_diff_css(uint32_t element_id,
                           const std::unordered_map<std::string, std::string>& inline_styles,
                           const std::unordered_map<std::string, std::string>& user_stylesheet,
                           const std::unordered_map<std::string, std::string>& user_agent_stylesheet) {
        auto computed = get_computed_style(element_id, inline_styles, user_stylesheet, user_agent_stylesheet);
        std::ostringstream oss;
        
        for (const auto& [prop, style] : computed) {
            // Only show properties that differ from default
            if (style.value != default_style_properties()[prop].value || prop.find("--") == 0) {
                oss << prop << ": " << style.value << "\n";
            }
        }
        
        return oss.str();
    }

private:
    // Expand shorthand properties (margin, padding, border, etc.)
    void expand_shorthand_properties(std::unordered_map<std::string, StyleProperty>& computed) {
        // margin: 10px 20px 30px 40px -> margin-top, margin-right, margin-bottom, margin-left
        expand_shorthand("margin", computed, {"margin-top", "margin-right", "margin-bottom", "margin-left"});
        expand_shorthand("padding", computed, {"padding-top", "padding-right", "padding-bottom", "padding-left"});
        expand_shorthand("border-width", computed, {"border-top-width", "border-right-width", "border-bottom-width", "border-left-width"});
        
        // font: 16px/1.5 sans-serif
        if (computed.find("font") != computed.end()) {
            std::string font_value = computed["font"].value;
            // Simplified parsing
            size_t slash_pos = font_value.find('/');
            if (slash_pos != std::string::npos) {
                computed["line-height"].value = font_value.substr(slash_pos + 1);
            }
        }
    }
    
    void expand_shorthand(const std::string& shorthand, std::unordered_map<std::string, StyleProperty>& computed,
                        const std::vector<std::string>& expanded_properties) {
        auto it = computed.find(shorthand);
        if (it != computed.end() && it->second.value != "initial") {
            std::string value = it->second.value;
            std::vector<std::string> parts = split_css_value(value);
            
            if (parts.size() == 1) {
                for (const auto& prop : expanded_properties) {
                    computed[prop].value = parts[0];
                }
            } else if (parts.size() == 2) {
                computed[expanded_properties[0]].value = parts[0];
                computed[expanded_properties[1]].value = parts[0];
                computed[expanded_properties[2]].value = parts[1];
                computed[expanded_properties[3]].value = parts[1];
            } else if (parts.size() >= 3) {
                computed[expanded_properties[0]].value = parts[0];
                computed[expanded_properties[1]].value = parts[1];
                computed[expanded_properties[2]].value = parts[2];
                if (parts.size() > 3) {
                    computed[expanded_properties[3]].value = parts[3];
                } else {
                    computed[expanded_properties[3]].value = parts[0];
                }
            }
        }
    }
    
    // Apply CSS inheritance for inherited properties
    void apply_inheritance(std::unordered_map<std::string, StyleProperty>& computed, uint32_t element_id) {
        // Walk computed styles and apply inheritance to children
        for (auto& [prop, style] : computed) {
            if (style.is_inherited) {
                // Mark as inheritable
                style.is_inherited_from = "parent";
            }
        }
    }
    
    std::vector<std::string> split_css_value(const std::string& value) {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = value.find(' ');
        
        while (end != std::string::npos) {
            std::string part = trim_css(value.substr(start, end - start));
            if (!part.empty()) {
                parts.push_back(part);
            }
            start = end + 1;
            end = value.find(' ', start);
        }
        
        std::string last_part = trim_css(value.substr(start));
        if (!last_part.empty()) {
            parts.push_back(last_part);
        }
        
        return parts;
    }
    
    std::string trim_css(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
    
    mutable std::mutex mutex_;
};

// CSS specificity calculation
class SpecificityCalculator {
public:
    struct Specificity {
        int a; // inline styles
        int b; // IDs
        int c; // classes, attributes, pseudo-classes
        int d; // elements, pseudo-elements
    };
    
    Specificity parse_selector(const std::string& selector) {
        Specificity spec = {0, 0, 0, 0};
        std::string s = normalize_selector(selector);
        
        // Count #id
        spec.b += count_occurrences(s, "#[a-zA-Z_][a-zA-Z0-9_-]*");
        // Count .class, :pseudo, [attr]
        spec.c += count_occurrences(s, "\.[a-zA-Z_][a-zA-Z0-9_-]*");
        spec.c += count_occurrences(s, ":[a-zA-Z-]+");
        spec.c += count_occurrences(s, "\[[^\]]+\]");
        // Count elements
        spec.d += count_occurrences(s, "[a-zA-Z]+[a-zA-Z0-9-]*");
        
        // Inline style counts as 'a' level
        return spec;
    }
    
    bool is_more_specific(const Specificity& a, const Specificity& b) {
        if (a.a != b.a) return a.a > b.a;
        if (a.b != b.b) return a.b > b.b;
        if (a.c != b.c) return a.c > b.c;
        return a.d >= b.d;
    }
    
    std::string specificity_to_string(const Specificity& spec) {
        return std::to_string(spec.a) + "," + std::to_string(spec.b) + "," + 
               std::to_string(spec.c) + "," + std::to_string(spec.d);
    }

private:
    std::string normalize_selector(const std::string& selector) {
        std::string s = selector;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }
    
    int count_occurrences(const std::string& str, const std::string& pattern) {
        std::regex re(pattern);
        auto begin = std::sregex_iterator(str.begin(), str.end(), re);
        auto end = std::sregex_iterator();
        return std::distance(begin, end);
    }
};

// CSS rule inspector
class CssRuleInspector {
public:
    struct RuleMatch {
        uint32_t specificity_a : 16;
        uint32_t specificity_b : 16;
        uint32_t specificity_c : 16;
        uint32_t specificity_d : 16;
        std::string selector;
        std::string styles;
        std::string source;
        bool is_active;
    };
    
    void add_rule(uint32_t rule_id, const std::string& selector, const std::string& styles,
                const std::string& source = "author", bool is_active = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        SpecificityCalculator::Specificity spec = specificity_.parse_selector(selector);
        
        RuleMatch match;
        match.specificity_a = spec.a;
        match.specificity_b = spec.b;
        match.specificity_c = spec.c;
        match.specificity_d = spec.d;
        match.selector = selector;
        match.styles = styles;
        match.source = source;
        match.is_active = is_active;
        
        rules_[rule_id] = match;
    }
    
    std::vector<RuleMatch> get_applicable_rules(const std::string& selector) {
        std::lock_guard<std::mutex> lock(mutex_);
        SpecificityCalculator::Specificity target_spec = specificity_.parse_selector(selector);
        
        std::vector<RuleMatch> applicable;
        for (const auto& [id, rule] : rules_) {
            SpecificityCalculator::Specificity rule_spec = {
                rule.specificity_a, rule.specificity_b, rule.specificity_c, rule.specificity_d
            };
            if (specificity_.is_more_specific(target_spec, rule_spec)) {
                applicable.push_back(rule);
            }
        }
        return applicable;
    }
    
    std::vector<RuleMatch> get_all_rules() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<RuleMatch> all;
        for (const auto& [id, rule] : rules_) {
            all.push_back(rule);
        }
        return all;
    }

private:
    std::unordered_map<uint32_t, RuleMatch> rules_;
    SpecificityCalculator specificity_;
    mutable std::mutex mutex_;
};

} // namespace hyperion::debug::css