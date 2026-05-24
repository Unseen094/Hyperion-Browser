#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <regex>
#include <mutex>

namespace hyperion::css::container {

// Container query syntax for @container rule
enum class QueryOperator : uint8_t {
    MIN_WIDTH,
    MAX_WIDTH,
    MIN_HEIGHT,
    MAX_HEIGHT,
    EQUALS,
    LESS_THAN,
    GREATER_THAN,
};

struct ContainerCondition {
    bool negate;
    QueryOperator op;
    std::string value;
};

struct ContainerSizing {
    std::string size;
    std::string inline_size;
    bool has_size : 1;
    bool has_inline_size : 1;
};

class ContainerQueryParser {
public:
    // Parse @container rule
    bool parse_container_rule(const std::string& rule, 
                           std::vector<ContainerCondition>& conditions, 
                           ContainerSizing& sizing) {
        // Simplified container query parsing: (width >= 600px) and min-inline-size: 200px
        std::string normalized = normalize_css(rule);
        
        // Extract sizing
        if (normalized.find("size:") != std::string::npos) {
            sizing.has_size = parse_contain_size(normalized, "size", sizing.size);
        }
        if (normalized.find("inline-size:") != std::string::npos) {
            sizing.has_inline_size = parse_contain_size(normalized, "inline-size", sizing.inline_size);
        }
        
        // Extract query conditions: (min-width: 650px)
        std::regex query_re("\(([^)]+)\)");
        auto query_begin = std::sregex_iterator(normalized.begin(), normalized.end(), query_re);
        auto query_end = std::srex_iterator();
        
        for (auto i = query_begin; i != query_end; ++i) {
            std::string condition = (*i)[1];
            parse_condition(condition, conditions);
        }
        
        return !conditions.empty() || sizing.has_size || sizing.has_inline_size;
    }
    
    // Evaluate container query against element size
    bool evaluate_query(const std::vector<ContainerCondition>& conditions, 
                     const ContainerSizing& sizing, 
                     uint32_t width, uint32_t height,
                     uint32_t inline_width) {
        for (const auto& cond : conditions) {
            if (!evaluate_condition(cond, width, height, inline_width)) {
                return false; // Any condition fails = query fails
            }
        }
        
        // Check sizing constraints
        if (sizing.has_size && !check_size_constraint(sizing.size, width, height)) {
            return false;
        }
        if (sizing.has_inline_size && !check_size_constraint(sizing.inline_size, inline_width, 0)) {
            return false;
        }
        
        return true;
    }
    
private:
    std::string normalize_css(const std::string& rule) {
        std::regex space_re("\\s+");
        std::string normalized = std::regex_replace(rule, space_re, " ");
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        return normalized;
    }
    
    void parse_condition(const std::string& condition, std::vector<ContainerCondition>& conditions) {
        // Examples: min-width: 650px, > 30rem, width >= 40em
        std::string cont = condition;
        
        bool negate = false;
        if (cont.find("not ") == 0) {
            negate = true;
            cont = cont.substr(4);
        }
        
        // min-width: 650px
        size_t colon = cont.find(':');
        if (colon != std::string::npos) {
            std::string feature = cont.substr(0, colon);
            std::string value = cont.substr(colon + 1);
            trim(feature);
            trim(value);
            
            QueryOperator op = determine_operator(feature);
            conditions.push_back(ContainerCondition{negate, op, value});
            return;
        }
        
        // > 30rem
        colon = cont.find(' ');
        if (colon != std::string::npos) {
            std::string comparator = cont.substr(0, colon);
            std::string val = cont.substr(colon + 1);
            trim(comparator);
            trim(val);
            
            QueryOperator op = comparator == "<" ? QUERY_OP_LESS : 
                             comparator == ">" ? QUERY_OP_GREATER : 
                             comparator == "<=" ? QUERY_OP_MAX : 
                             comparator == ">=" ? QUERY_OP_MIN : 
                             QUERY_OP_EQUALS;
            conditions.push_back(ContainerCondition{negate, op, val});
        }
    }
    
    bool evaluate_condition(const ContainerCondition& cond, uint32_t width, uint32_t height,
                         uint32_t inline_width) {
        if (!negate_logic && cond.negate) {
            return false; // Negation prevents match, handled by container scope
        }
        
        uint32_t measured = 0;
        switch (cond.op) {
            case QUERY_OP_MIN_WIDTH: measured = width; break;
            case QUERY_OP_MAX_WIDTH: measured = width; break;
            case QUERY_OP_MIN_HEIGHT: measured = height; break;
            case QUERY_OP_MAX_HEIGHT: measured = height; break;
            case QUERY_OP_EQUALS: return width == std::stoi(cond.value);
            default: return false;
        }
        
        std::string val = cond.value;
        val.erase(std::remove_if(val.begin(), val.end(), [](char c) { return c == 'p' || c == 'x'; }), val.end());
        int value = std::stoi(val);
        
        switch (cond.op) {
            case MIN_WIDTH: return width >= value;
            case MAX_WIDTH: return width <= value;
            case MIN_HEIGHT: return height >= value;
            case MAX_HEIGHT: return height <= value;
            default: return false;
        }
    }
    
    bool parse_contain_size(const std::string& rule, const std::string& feature, std::string& out) {
        std::regex feature_re(feature + ":\\s*([^;]+)");
        std::smatch match;
        if (std::regex_search(rule, match, feature_re)) {
            out = match[1];
            trim(out);
            return true;
        }
        return false;
    }
    
    bool check_size_constraint(const std::string& constraint, int width, int height) {
        if (constraint.find("width") != std::string::npos) {
            int value = parse_css_measure(constraint);
            return width >= value;
        }
        if (constraint.find("height") != std::string::npos) {
            int value = parse_css_measure(constraint);
            return height >= value;
        }
        if (constraint.find("inline") != std::string::npos) {
            int value = parse_css_measure(constraint);
            return width >= value;
        }
        return true;
    }
    
    int parse_css_measure(const std::string& text) {
        std::string numeric;
        for (char c : text) {
            if (isdigit(c) || c == '.') numeric += c;
        }
        float val = std::stof(numeric);
        if (text.find("px") != std::string::npos) return static_cast<int>(val);
        if (text.find("em") != std::string::npos) return static_cast<int>(val * 16);
        if (text.find("rem") != std::string::npos) return static_cast<int>(val * 16); // Root em
        if (text.find("%") != std::string::npos) return static_cast<int>(val * 100); // Relative, default units
        return static_cast<int>(val);
    }
    
    QueryOperator determine_operator(const std::string& feature) {
        if (feature.find("min-width") != std::string::npos) return MIN_WIDTH;
        if (feature.find("max-width") != std::string::npos) return MAX_WIDTH;
        if (feature.find("min-height") != std::string::npos) return MIN_HEIGHT;
        if (feature.find("max-height") != std::string::npos) return MAX_HEIGHT;
        if (feature.find("width:") == 0) return EQUALS;
        return EQUALS;
    }
    
    void trim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
    }
};

// Container query registry - tracks container elements and their queries
class ContainerQueryRegistry {
public:
    // Register a container element with its query
    void register_container(uint32_t element_id, const std::string& query) {
        std::lock_guard<std::mutex> lock(mutex_);
        ContainerEntry entry;
        entry.query = query;
        parse_entry(query, entry);
        containers_[element_id] = entry;
    }
    
    // Evaluate all container queries for element
    std::vector<uint32_t> evaluate_queries(uint32_t element_id, uint32_t width, uint32_t height) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint32_t> matching;
        auto it = containers_.find(element_id);
        if (it != containers_.end()) {
            uint32_t inline_width = width; // Simplified: inline width = width for now
            bool matches = name::evaluate_query(it->second.conditions, it->second.sizing, width, height, inline_width);
            if (matches) matching.push_back(element_id);
        }
        return matching;
    }
    
    // Update container size
    void update_container_size(uint32_t element_id, uint32_t width, uint32_t height) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = containers_.find(element_id);
        if (it != containers_.end()) {
            it->second.width = width;
            it->second.height = height;
            evaluate_all(); // Re-evaluate after resize
        }
    }
    
private:
    struct ContainerEntry {
        std::string query;
        std::vector<ContainerCondition> conditions;
        ContainerSizing sizing;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    
    std::unordered_map<uint32_t, ContainerEntry> containers_;
    mutable std::mutex mutex_;
};
