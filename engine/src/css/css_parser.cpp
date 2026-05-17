#include <hre/css/css_parser.hpp>
#include <cwctype>

namespace hre::css {

css_parser::css_parser(std::wstring input) : m_input(std::move(input)) {}

wchar_t css_parser::consume() {
    if (m_pos >= m_input.length()) return L'\0';
    return m_input[m_pos++];
}

wchar_t css_parser::peek() const {
    if (m_pos >= m_input.length()) return L'\0';
    return m_input[m_pos];
}

void css_parser::skip_whitespace() {
    while (std::iswspace(peek())) consume();
}

std::wstring css_parser::consume_identifier() {
    std::wstring result;
    while (std::iswalnum(peek()) || peek() == L'-' || peek() == L'_') {
        result += consume();
    }
    return result;
}

stylesheet css_parser::parse() {
    stylesheet ss;
    while (m_pos < m_input.length()) {
        skip_whitespace();
        if (m_pos >= m_input.length()) break;
        ss.rules.push_back(parse_rule());
    }
    return ss;
}

rule css_parser::parse_rule() {
    rule r;
    while (peek() != L'{' && m_pos < m_input.length()) {
        r.selectors.push_back(parse_selector());
        skip_whitespace();
        if (peek() == L',') consume();
        skip_whitespace();
    }

    if (consume() == L'{') {
        while (peek() != L'}' && m_pos < m_input.length()) {
            skip_whitespace();
            r.declarations.push_back(parse_declaration());
            skip_whitespace();
            if (peek() == L';') consume();
            skip_whitespace();
        }
        consume(); // Consume '}'
    }

    return r;
}

selector css_parser::parse_selector() {
    selector s;
    if (peek() == L'.') {
        consume();
        s.class_name = consume_identifier();
    } else if (peek() == L'#') {
        consume();
        s.id = consume_identifier();
    } else {
        s.tag_name = consume_identifier();
    }
    return s;
}

declaration css_parser::parse_declaration() {
    declaration d;
    d.name = consume_identifier();
    skip_whitespace();
    if (consume() == L':') {
        skip_whitespace();
        d.value.type = value_type::keyword;
        while (peek() != L';' && peek() != L'}' && m_pos < m_input.length()) {
            d.value.data += consume();
        }
        // Basic length parsing
        if (d.value.data.find(L"px") != std::wstring::npos) {
            d.value.type = value_type::length;
            try {
                d.value.number = std::stof(d.value.data);
            } catch (...) {}
        }
    }
    return d;
}

} // namespace hre::css
