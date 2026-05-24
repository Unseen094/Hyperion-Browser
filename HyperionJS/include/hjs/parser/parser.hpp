#pragma once

#include <hjs/lexer/token.hpp>
#include <hjs/parser/ast.hpp>
#include <vector>
#include <memory>
#include <string>
#include <initializer_list>
#include <iostream>

namespace hjs::parser {

class Parser {
    using TT = hjs::lexer::TokenType;
    using Value = hjs::Value;
public:
    explicit Parser(std::vector<hjs::lexer::Token> tokens);
    std::vector<std::unique_ptr<hjs::ast::Stmt>> parse();

private:
    // Declarations
    std::unique_ptr<ast::Stmt> declaration();
    std::unique_ptr<ast::Stmt> var_declaration(bool is_const = false);
    std::unique_ptr<ast::Stmt> function_declaration(const std::wstring& kind);
    std::unique_ptr<ast::Stmt> class_declaration();

    // Statements
    std::unique_ptr<ast::Stmt> statement();
    std::unique_ptr<ast::Stmt> if_statement();
    std::unique_ptr<ast::Stmt> while_statement();
    std::unique_ptr<ast::Stmt> for_statement();
    std::unique_ptr<ast::Stmt> return_statement();
    std::unique_ptr<ast::Stmt> throw_statement();
    std::unique_ptr<ast::Stmt> try_statement();
    std::unique_ptr<ast::Stmt> print_statement();
    std::unique_ptr<ast::Stmt> expression_statement();
    std::vector<std::unique_ptr<ast::Stmt>> block();

    // Expressions
    std::unique_ptr<ast::Expr> expression();
    std::unique_ptr<ast::Expr> assignment();
    std::unique_ptr<ast::Expr> ternary();
    std::unique_ptr<ast::Expr> logical_or();
    std::unique_ptr<ast::Expr> logical_and();
    std::unique_ptr<ast::Expr> equality();
    std::unique_ptr<ast::Expr> comparison();
    std::unique_ptr<ast::Expr> term();
    std::unique_ptr<ast::Expr> factor();
    std::unique_ptr<ast::Expr> unary();
    std::unique_ptr<ast::Expr> postfix();
    std::unique_ptr<ast::Expr> call();
    std::unique_ptr<ast::Expr> finish_call(std::unique_ptr<ast::Expr> callee);
    std::unique_ptr<ast::Expr> primary();
    std::unique_ptr<ast::Expr> await_expression();
    std::unique_ptr<ast::Expr> parse_arrow_body(std::vector<std::wstring> params);

    // Helpers
    std::vector<std::wstring> parse_params();
    void consume_semicolon();
    void synchronize();
    bool match(std::initializer_list<TT> types);
    bool check(TT type) const;
    hjs::lexer::Token advance();
    bool is_at_end() const;
    hjs::lexer::Token peek() const;
    hjs::lexer::Token previous() const;
    hjs::lexer::Token consume(TT type, const std::string& message);

    std::vector<hjs::lexer::Token> m_tokens;
    size_t m_current = 0;
};

} // namespace hjs::parser
