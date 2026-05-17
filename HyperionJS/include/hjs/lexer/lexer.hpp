#pragma once

#include <hjs/lexer/token.hpp>
#include <vector>
#include <string>

namespace hjs::lexer {

class Lexer {
public:
    explicit Lexer(std::wstring source);
    
    std::vector<Token> tokenize();

private:
    bool is_at_end() const;
    wchar_t advance();
    wchar_t peek() const;
    wchar_t peek_next() const;
    bool match(wchar_t expected);
    
    void scan_token();
    void identifier();
    void number();
    void string(wchar_t quote);
    void template_string();
    void add_token(TokenType type);
    void add_token(TokenType type, std::wstring lexeme);

    std::wstring m_source;
    std::vector<Token> m_tokens;
    int m_start = 0;
    int m_current = 0;
    int m_line = 1;
    int m_column = 1;
};

} // namespace hjs::lexer
