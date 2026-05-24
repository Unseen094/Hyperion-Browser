#pragma once

#include <string>
#include <string_view>

namespace hjs::lexer {

enum class TokenType {
    // Single-character tokens
    LeftParen, RightParen, LeftBrace, RightBrace, LeftBracket, RightBracket,
    Comma, Dot, Semicolon, Colon, Tilde, QuestionMark,

    // Arithmetic
    Plus, Minus, Star, Slash, Percent,

    // Compound assignment
    PlusEqual, MinusEqual, StarEqual, SlashEqual, PercentEqual,

    // Increment / Decrement
    PlusPlus, MinusMinus,

    // Comparison
    Bang, BangEqual, BangEqualEqual,
    Equal, EqualEqual, EqualEqualEqual,
    Greater, GreaterEqual,
    Less, LessEqual,

    // Logical
    AmpAmp,   // &&
    PipePipe, // ||

    // Arrow
    Arrow,    // =>

    // Bitwise
    Amp, Pipe, Caret, LessLess, GreaterGreater,

    // Spread / Rest
    DotDotDot,

    // Literals
    Identifier, String, Number, TemplateString,

    // Keywords
    And, Class, Else, False, Fun, For, If, Nil, Or,
    Print, Return, Super, This, True, Var, While,
    Let, Const, New, Typeof, Instanceof,
    Break, Continue, Throw, Try, Catch, Finally,
    In, Of, Import, Export, Default, Delete, Void,
    Switch, Case, Extends, Static,
    Async, Await,

    EndOfFile,
    Error
};

struct Token {
    TokenType type;
    std::wstring lexeme;
    int line;
    int column;

    Token(TokenType type, std::wstring lexeme, int line, int column)
        : type(type), lexeme(std::move(lexeme)), line(line), column(column) {}
};

} // namespace hjs::lexer
