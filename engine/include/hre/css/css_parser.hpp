#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace hre::css {

enum class value_type {
    keyword,
    color,
    length,
    percentage
};

struct css_value {
    value_type type;
    std::wstring data;
    float number = 0.0f;
};

struct declaration {
    std::wstring name;
    css_value value;
};

struct selector {
    std::wstring tag_name;
    std::wstring class_name;
    std::wstring id;
};

struct rule {
    std::vector<selector> selectors;
    std::vector<declaration> declarations;
};

struct stylesheet {
    std::vector<rule> rules;
};

class css_parser {
public:
    explicit css_parser(std::wstring input);
    stylesheet parse();

private:
    std::wstring m_input;
    size_t m_pos = 0;

    wchar_t consume();
    wchar_t peek() const;
    void skip_whitespace();
    std::wstring consume_identifier();
    
    rule parse_rule();
    selector parse_selector();
    declaration parse_declaration();
};

} // namespace hre::css
