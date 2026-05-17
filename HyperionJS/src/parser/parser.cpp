#include <hjs/parser/parser.hpp>
#include <stdexcept>

namespace hjs::parser {

Parser::Parser(std::vector<hjs::lexer::Token> tokens) : m_tokens(std::move(tokens)) {}

std::vector<std::unique_ptr<hjs::ast::Stmt>> Parser::parse() {
    std::vector<std::unique_ptr<hjs::ast::Stmt>> stmts;
    while (!is_at_end()) {
        if (match({TT::Semicolon})) continue;
        auto s = declaration();
        if (s) stmts.push_back(std::move(s));
    }
    return stmts;
}

// ---- Declarations -------------------------------------------------------

std::unique_ptr<ast::Stmt> Parser::declaration() {
    try {
        if (match({TT::Fun}))   return function_declaration(L"function");
        if (match({TT::Class})) return class_declaration();
        if (match({TT::Var}) || match({TT::Let}) || match({TT::Const})) {
            bool is_const = previous().type == TT::Const;
            return var_declaration(is_const);
        }
        return statement();
    } catch (const std::runtime_error& e) {
        std::wcerr << L"Parser Error: " << e.what()
                   << L" at '" << peek().lexeme << L"'\n";
        synchronize();
        return nullptr;
    }
}

std::unique_ptr<ast::Stmt> Parser::var_declaration(bool is_const) {
    auto name = consume(TT::Identifier, "Expect variable name.");
    std::unique_ptr<ast::Expr> init = nullptr;
    if (match({TT::Equal})) init = expression();
    consume_semicolon();
    return std::make_unique<ast::VarStmt>(name.lexeme, std::move(init), is_const);
}

std::unique_ptr<ast::Stmt> Parser::function_declaration(const std::wstring&) {
    auto name = consume(TT::Identifier, "Expect function name.");
    consume(TT::LeftParen, "Expect '(' after function name.");
    auto params = parse_params();
    consume(TT::LeftBrace, "Expect '{' before function body.");
    auto body = block();
    return std::make_unique<ast::FunctionStmt>(name.lexeme, std::move(params), std::move(body));
}

std::unique_ptr<ast::Stmt> Parser::class_declaration() {
    auto name = consume(TT::Identifier, "Expect class name.");
    std::wstring super;
    if (match({TT::Extends})) {
        super = consume(TT::Identifier, "Expect superclass name.").lexeme;
    }
    consume(TT::LeftBrace, "Expect '{' before class body.");
    std::vector<ast::ClassMethod> methods;
    while (!check(TT::RightBrace) && !is_at_end()) {
        bool is_static = match({TT::Static});
        auto mname = consume(TT::Identifier, "Expect method name.");
        consume(TT::LeftParen, "Expect '(' after method name.");
        auto params = parse_params();
        consume(TT::LeftBrace, "Expect '{' before method body.");
        auto body = block();
        methods.push_back({mname.lexeme, std::move(params), std::move(body), is_static});
    }
    consume(TT::RightBrace, "Expect '}' after class body.");
    return std::make_unique<ast::ClassStmt>(name.lexeme, super, std::move(methods));
}

// ---- Statements ---------------------------------------------------------

std::unique_ptr<ast::Stmt> Parser::statement() {
    if (match({TT::If}))       return if_statement();
    if (match({TT::While}))    return while_statement();
    if (match({TT::For}))      return for_statement();
    if (match({TT::Print}))    return print_statement();
    if (match({TT::Return}))   return return_statement();
    if (match({TT::Break}))    { consume_semicolon(); return std::make_unique<ast::BreakStmt>(); }
    if (match({TT::Continue})) { consume_semicolon(); return std::make_unique<ast::ContinueStmt>(); }
    if (match({TT::Throw}))    return throw_statement();
    if (match({TT::Try}))      return try_statement();
    if (match({TT::LeftBrace})) return std::make_unique<ast::BlockStmt>(block());
    return expression_statement();
}

std::unique_ptr<ast::Stmt> Parser::if_statement() {
    consume(TT::LeftParen, "Expect '(' after 'if'.");
    auto cond = expression();
    consume(TT::RightParen, "Expect ')' after condition.");
    auto then_ = statement();
    std::unique_ptr<ast::Stmt> else_ = nullptr;
    if (match({TT::Else})) else_ = statement();
    return std::make_unique<ast::IfStmt>(std::move(cond), std::move(then_), std::move(else_));
}

std::unique_ptr<ast::Stmt> Parser::while_statement() {
    consume(TT::LeftParen, "Expect '(' after 'while'.");
    auto cond = expression();
    consume(TT::RightParen, "Expect ')' after condition.");
    auto body = statement();
    return std::make_unique<ast::WhileStmt>(std::move(cond), std::move(body));
}

std::unique_ptr<ast::Stmt> Parser::for_statement() {
    consume(TT::LeftParen, "Expect '(' after 'for'.");

    std::unique_ptr<ast::Stmt> init;
    if (match({TT::Semicolon})) { init = nullptr; }
    else if (match({TT::Var}) || match({TT::Let}) || match({TT::Const})) {
        init = var_declaration(previous().type == TT::Const);
    } else {
        init = expression_statement();
    }

    std::unique_ptr<ast::Expr> cond = nullptr;
    if (!check(TT::Semicolon)) cond = expression();
    consume(TT::Semicolon, "Expect ';' after loop condition.");

    std::unique_ptr<ast::Expr> incr = nullptr;
    if (!check(TT::RightParen)) incr = expression();
    consume(TT::RightParen, "Expect ')' after for clauses.");

    auto body = statement();

    // De-sugar: append increment to body
    if (incr) {
        std::vector<std::unique_ptr<ast::Stmt>> stmts;
        stmts.push_back(std::move(body));
        stmts.push_back(std::make_unique<ast::ExprStmt>(std::move(incr)));
        body = std::make_unique<ast::BlockStmt>(std::move(stmts));
    }

    // Wrap in while
    if (!cond) cond = std::make_unique<ast::LiteralExpr>(Value(true));
    body = std::make_unique<ast::WhileStmt>(std::move(cond), std::move(body));

    // Wrap init
    if (init) {
        std::vector<std::unique_ptr<ast::Stmt>> stmts;
        stmts.push_back(std::move(init));
        stmts.push_back(std::move(body));
        body = std::make_unique<ast::BlockStmt>(std::move(stmts));
    }
    return body;
}

std::unique_ptr<ast::Stmt> Parser::return_statement() {
    std::unique_ptr<ast::Expr> val = nullptr;
    if (!check(TT::Semicolon) && !check(TT::RightBrace) && !is_at_end()) {
        val = expression();
    }
    consume_semicolon();
    return std::make_unique<ast::ReturnStmt>(std::move(val));
}

std::unique_ptr<ast::Stmt> Parser::throw_statement() {
    auto val = expression();
    consume_semicolon();
    return std::make_unique<ast::ThrowStmt>(std::move(val));
}

std::unique_ptr<ast::Stmt> Parser::try_statement() {
    consume(TT::LeftBrace, "Expect '{' after 'try'.");
    auto body = std::make_unique<ast::BlockStmt>(block());

    std::wstring catch_var;
    std::unique_ptr<ast::Stmt> catch_body = nullptr;
    if (match({TT::Catch})) {
        consume(TT::LeftParen, "Expect '(' after 'catch'.");
        catch_var = consume(TT::Identifier, "Expect catch variable.").lexeme;
        consume(TT::RightParen, "Expect ')' after catch var.");
        consume(TT::LeftBrace, "Expect '{' after catch.");
        catch_body = std::make_unique<ast::BlockStmt>(block());
    }

    std::unique_ptr<ast::Stmt> finally_body = nullptr;
    if (match({TT::Finally})) {
        consume(TT::LeftBrace, "Expect '{' after 'finally'.");
        finally_body = std::make_unique<ast::BlockStmt>(block());
    }

    return std::make_unique<ast::TryStmt>(std::move(body), catch_var,
                                          std::move(catch_body), std::move(finally_body));
}

std::unique_ptr<ast::Stmt> Parser::print_statement() {
    auto val = expression();
    consume_semicolon();
    return std::make_unique<ast::PrintStmt>(std::move(val));
}

std::unique_ptr<ast::Stmt> Parser::expression_statement() {
    auto expr = expression();
    consume_semicolon();
    return std::make_unique<ast::ExprStmt>(std::move(expr));
}

// ---- Expression hierarchy -----------------------------------------------

std::unique_ptr<ast::Expr> Parser::expression() { return assignment(); }

std::unique_ptr<ast::Expr> Parser::assignment() {
    auto expr = ternary();

    // Compound assignment: +=, -=, *=, /=, %=
    if (match({TT::PlusEqual, TT::MinusEqual, TT::StarEqual, TT::SlashEqual, TT::PercentEqual})) {
        std::wstring op = previous().lexeme;
        auto val = assignment();
        if (auto* ve = dynamic_cast<ast::VariableExpr*>(expr.get())) {
            return std::make_unique<ast::CompoundAssignExpr>(ve->name, op, std::move(val));
        }
        throw std::runtime_error("Invalid compound assignment target.");
    }

    if (match({TT::Equal})) {
        auto val = assignment();
        if (auto* ve = dynamic_cast<ast::VariableExpr*>(expr.get()))
            return std::make_unique<ast::AssignExpr>(ve->name, std::move(val));
        if (auto* me = dynamic_cast<ast::MemberExpr*>(expr.get()))
            return std::make_unique<ast::SetExpr>(std::move(me->object), me->property, std::move(val));
        if (auto* ie = dynamic_cast<ast::IndexExpr*>(expr.get()))
            return std::make_unique<ast::SetIndexExpr>(std::move(ie->object), std::move(ie->index), std::move(val));
        throw std::runtime_error("Invalid assignment target.");
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::ternary() {
    auto expr = logical_or();
    if (match({TT::QuestionMark})) {
        auto then_ = expression();
        consume(TT::Colon, "Expect ':' in ternary.");
        auto else_ = expression();
        return std::make_unique<ast::TernaryExpr>(std::move(expr), std::move(then_), std::move(else_));
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::logical_or() {
    auto expr = logical_and();
    while (match({TT::PipePipe})) {
        auto right = logical_and();
        expr = std::make_unique<ast::LogicalExpr>(std::move(expr), L"||", std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::logical_and() {
    auto expr = equality();
    while (match({TT::AmpAmp})) {
        auto right = equality();
        expr = std::make_unique<ast::LogicalExpr>(std::move(expr), L"&&", std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::equality() {
    auto expr = comparison();
    while (match({TT::BangEqual, TT::BangEqualEqual, TT::EqualEqual, TT::EqualEqualEqual})) {
        std::wstring op = previous().lexeme;
        auto right = comparison();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::comparison() {
    auto expr = term();
    while (match({TT::Greater, TT::GreaterEqual, TT::Less, TT::LessEqual})) {
        std::wstring op = previous().lexeme;
        auto right = term();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::term() {
    auto expr = factor();
    while (match({TT::Minus, TT::Plus})) {
        std::wstring op = previous().lexeme;
        auto right = factor();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::factor() {
    auto expr = unary();
    while (match({TT::Slash, TT::Star, TT::Percent})) {
        std::wstring op = previous().lexeme;
        auto right = unary();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::unary() {
    if (match({TT::Bang, TT::Minus})) {
        std::wstring op = previous().lexeme;
        return std::make_unique<ast::UnaryExpr>(op, unary(), true);
    }
    if (match({TT::Typeof})) {
        return std::make_unique<ast::TypeofExpr>(unary());
    }
    // Pre-increment / pre-decrement
    if (match({TT::PlusPlus, TT::MinusMinus})) {
        std::wstring op = previous().lexeme;
        return std::make_unique<ast::UnaryExpr>(op, unary(), true);
    }
    return postfix();
}

std::unique_ptr<ast::Expr> Parser::postfix() {
    auto expr = call();
    if (match({TT::PlusPlus, TT::MinusMinus})) {
        std::wstring op = previous().lexeme;
        return std::make_unique<ast::PostfixExpr>(std::move(expr), op);
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::call() {
    auto expr = primary();
    while (true) {
        if (match({TT::LeftParen})) {
            expr = finish_call(std::move(expr));
        } else if (match({TT::Dot})) {
            auto name = consume(TT::Identifier, "Expect property name.");
            expr = std::make_unique<ast::MemberExpr>(std::move(expr), name.lexeme);
        } else if (match({TT::LeftBracket})) {
            auto idx = expression();
            consume(TT::RightBracket, "Expect ']'.");
            expr = std::make_unique<ast::IndexExpr>(std::move(expr), std::move(idx));
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<ast::Expr> Parser::finish_call(std::unique_ptr<ast::Expr> callee) {
    std::vector<std::unique_ptr<ast::Expr>> args;
    if (!check(TT::RightParen)) {
        do { args.push_back(expression()); } while (match({TT::Comma}));
    }
    consume(TT::RightParen, "Expect ')' after arguments.");
    return std::make_unique<ast::CallExpr>(std::move(callee), std::move(args));
}

std::unique_ptr<ast::Expr> Parser::primary() {
    if (match({TT::False}))  return std::make_unique<ast::LiteralExpr>(Value(false));
    if (match({TT::True}))   return std::make_unique<ast::LiteralExpr>(Value(true));
    if (match({TT::Nil}))    return std::make_unique<ast::LiteralExpr>(Value());

    if (match({TT::Number}))
        return std::make_unique<ast::LiteralExpr>(Value(std::stod(previous().lexeme)));

    if (match({TT::String}))
        return std::make_unique<ast::LiteralExpr>(Value(previous().lexeme));

    if (match({TT::TemplateString}))
        return std::make_unique<ast::TemplateStringExpr>(previous().lexeme);

    if (match({TT::This}))
        return std::make_unique<ast::VariableExpr>(L"this");

    if (match({TT::New})) {
        auto callee = std::make_unique<ast::VariableExpr>(
            consume(TT::Identifier, "Expect class name after 'new'.").lexeme);
        std::vector<std::unique_ptr<ast::Expr>> args;
        if (match({TT::LeftParen})) {
            if (!check(TT::RightParen))
                do { args.push_back(expression()); } while (match({TT::Comma}));
            consume(TT::RightParen, "Expect ')' after new args.");
        }
        return std::make_unique<ast::NewExpr>(std::move(callee), std::move(args));
    }

    if (match({TT::Identifier})) {
        std::wstring name = previous().lexeme;
        // Arrow function: name => expr  or  name => { block }
        if (match({TT::Arrow})) {
            std::vector<std::wstring> params = {name};
            return parse_arrow_body(std::move(params));
        }
        return std::make_unique<ast::VariableExpr>(name);
    }

    if (match({TT::LeftParen})) {
        // Could be: (params) => body  or  (expr)
        // Check for arrow function
        if (check(TT::RightParen) || check(TT::Identifier)) {
            size_t saved = m_current;
            try {
                std::vector<std::wstring> params;
                if (!check(TT::RightParen)) {
                    do {
                        params.push_back(consume(TT::Identifier, "").lexeme);
                    } while (match({TT::Comma}));
                }
                consume(TT::RightParen, "");
                if (match({TT::Arrow})) {
                    return parse_arrow_body(std::move(params));
                }
                // Not an arrow — rewind
                m_current = saved;
            } catch (...) {
                m_current = saved;
            }
        }
        auto expr = expression();
        consume(TT::RightParen, "Expect ')' after expression.");
        return std::make_unique<ast::GroupingExpr>(std::move(expr));
    }

    if (match({TT::LeftBracket})) {
        std::vector<std::unique_ptr<ast::Expr>> elems;
        if (!check(TT::RightBracket)) {
            do { elems.push_back(expression()); } while (match({TT::Comma}));
        }
        consume(TT::RightBracket, "Expect ']'.");
        return std::make_unique<ast::ArrayLiteralExpr>(std::move(elems));
    }

    if (match({TT::LeftBrace})) {
        std::vector<ast::ObjectProperty> props;
        if (!check(TT::RightBrace)) {
            do {
                std::wstring key;
                if (match({TT::Identifier, TT::String})) key = previous().lexeme;
                else throw std::runtime_error("Expect property name.");
                consume(TT::Colon, "Expect ':' after key.");
                props.push_back({key, expression()});
            } while (match({TT::Comma}));
        }
        consume(TT::RightBrace, "Expect '}'.");
        return std::make_unique<ast::ObjectLiteralExpr>(std::move(props));
    }

    throw std::runtime_error("Expect expression.");
}

std::unique_ptr<ast::Expr> Parser::parse_arrow_body(std::vector<std::wstring> params) {
    if (match({TT::LeftBrace})) {
        auto body = block();
        return std::make_unique<ast::ArrowFunctionExpr>(
            std::move(params), nullptr, std::move(body));
    }
    auto body_expr = expression();
    return std::make_unique<ast::ArrowFunctionExpr>(
        std::move(params), std::move(body_expr),
        std::vector<std::unique_ptr<ast::Stmt>>{});
}

// ---- Helpers ------------------------------------------------------------

std::vector<std::wstring> Parser::parse_params() {
    std::vector<std::wstring> params;
    if (!check(TT::RightParen)) {
        do {
            params.push_back(consume(TT::Identifier, "Expect param name.").lexeme);
        } while (match({TT::Comma}));
    }
    consume(TT::RightParen, "Expect ')' after parameters.");
    return params;
}

std::vector<std::unique_ptr<ast::Stmt>> Parser::block() {
    std::vector<std::unique_ptr<ast::Stmt>> stmts;
    while (!check(TT::RightBrace) && !is_at_end()) {
        if (match({TT::Semicolon})) continue;
        auto s = declaration();
        if (s) stmts.push_back(std::move(s));
    }
    consume(TT::RightBrace, "Expect '}'.");
    return stmts;
}

void Parser::consume_semicolon() {
    // Automatic semicolon insertion: skip if present
    match({TT::Semicolon});
}

bool Parser::match(std::initializer_list<TT> types) {
    for (auto t : types) {
        if (check(t)) { advance(); return true; }
    }
    return false;
}

bool Parser::check(TT type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

hjs::lexer::Token Parser::advance() {
    if (!is_at_end()) m_current++;
    return previous();
}

bool Parser::is_at_end() const { return peek().type == TT::EndOfFile; }
hjs::lexer::Token Parser::peek()     const { return m_tokens[m_current]; }
hjs::lexer::Token Parser::previous() const { return m_tokens[m_current - 1]; }

hjs::lexer::Token Parser::consume(TT type, const std::string& msg) {
    if (check(type)) return advance();
    throw std::runtime_error(msg);
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TT::Semicolon) return;
        switch (peek().type) {
            case TT::Fun: case TT::Var: case TT::Let: case TT::Const:
            case TT::For: case TT::If: case TT::While: case TT::Return:
            case TT::Class: case TT::Try: case TT::Throw:
                return;
            default: break;
        }
        advance();
    }
}

} // namespace hjs::parser
