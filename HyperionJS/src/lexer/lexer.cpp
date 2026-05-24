#include <hjs/lexer/lexer.hpp>
#include <map>
#include <sstream>
#include <iostream>

namespace hjs::lexer {

namespace {
    const std::map<std::wstring, TokenType> g_keywords = {
        {L"break",      TokenType::Break},
        {L"case",       TokenType::Case},
        {L"catch",      TokenType::Catch},
        {L"class",      TokenType::Class},
        {L"const",      TokenType::Const},
        {L"continue",   TokenType::Continue},
        {L"default",    TokenType::Default},
        {L"delete",     TokenType::Delete},
        {L"else",       TokenType::Else},
        {L"export",     TokenType::Export},
        {L"extends",    TokenType::Extends},
        {L"false",      TokenType::False},
        {L"finally",    TokenType::Finally},
        {L"for",        TokenType::For},
        {L"function",   TokenType::Fun},
        {L"if",         TokenType::If},
        {L"import",     TokenType::Import},
        {L"in",         TokenType::In},
        {L"instanceof", TokenType::Instanceof},
        {L"let",        TokenType::Let},
        {L"new",        TokenType::New},
        {L"null",       TokenType::Nil},
        {L"of",         TokenType::Of},
        {L"print",      TokenType::Print},
        {L"return",     TokenType::Return},
        {L"static",     TokenType::Static},
        {L"super",      TokenType::Super},
        {L"switch",     TokenType::Switch},
        {L"this",       TokenType::This},
        {L"throw",      TokenType::Throw},
        {L"true",       TokenType::True},
        {L"try",        TokenType::Try},
        {L"typeof",     TokenType::Typeof},
        {L"undefined",  TokenType::Nil},
        {L"var",        TokenType::Var},
        {L"void",       TokenType::Void},
    {L"while", TokenType::While},
    {L"async", TokenType::Async},
    {L"await", TokenType::Await},
};
}

Lexer::Lexer(std::wstring source) : m_source(std::move(source)) {}

std::vector<Token> Lexer::tokenize() {
    // Skip hashbang (#!...) at start of file
    if (m_source.size() >= 2 && m_source[0] == L'#' && m_source[1] == L'!') {
        while (!is_at_end() && advance() != L'\n') {}
        m_line++;
        m_column = 1;
        m_start = m_current;
    }

    while (!is_at_end()) {
        m_start = m_current;
        scan_token();
    }
    m_tokens.emplace_back(TokenType::EndOfFile, L"", m_line, m_column);
    return m_tokens;
}

void Lexer::scan_token() {
    wchar_t c = advance();
    switch (c) {
        // Single-char
        case L'(': add_token(TokenType::LeftParen); break;
        case L')': add_token(TokenType::RightParen); break;
        case L'{': add_token(TokenType::LeftBrace); break;
        case L'}': add_token(TokenType::RightBrace); break;
        case L'[': add_token(TokenType::LeftBracket); break;
        case L']': add_token(TokenType::RightBracket); break;
        case L',': add_token(TokenType::Comma); break;
        case L';': add_token(TokenType::Semicolon); break;
        case L':': add_token(TokenType::Colon); break;
        case L'~': add_token(TokenType::Tilde); break;
        case L'?': add_token(TokenType::QuestionMark); break;
        case L'^': add_token(TokenType::Caret); break;

        // Dot / spread
        case L'.':
            if (peek() == L'.' && peek_next() == L'.') {
                advance(); advance();
                add_token(TokenType::DotDotDot);
            } else {
                add_token(TokenType::Dot);
            }
            break;

        // + ++ +=
        case L'+':
            if (match(L'+'))      add_token(TokenType::PlusPlus);
            else if (match(L'=')) add_token(TokenType::PlusEqual);
            else                  add_token(TokenType::Plus);
            break;

        // - -- -=
        case L'-':
            if (match(L'-'))      add_token(TokenType::MinusMinus);
            else if (match(L'=')) add_token(TokenType::MinusEqual);
            else                  add_token(TokenType::Minus);
            break;

        // * *=
        case L'*':
            add_token(match(L'=') ? TokenType::StarEqual : TokenType::Star);
            break;

        // % %=
        case L'%':
            add_token(match(L'=') ? TokenType::PercentEqual : TokenType::Percent);
            break;

        // ! != !==
        case L'!':
            if (match(L'=')) {
                add_token(match(L'=') ? TokenType::BangEqualEqual : TokenType::BangEqual);
            } else {
                add_token(TokenType::Bang);
            }
            break;

        // = == === =>
        case L'=':
            if (match(L'=')) {
                add_token(match(L'=') ? TokenType::EqualEqualEqual : TokenType::EqualEqual);
            } else if (match(L'>')) {
                add_token(TokenType::Arrow);
            } else {
                add_token(TokenType::Equal);
            }
            break;

        // < <= <<
        case L'<':
            if (match(L'='))      add_token(TokenType::LessEqual);
            else if (match(L'<')) add_token(TokenType::LessLess);
            else                  add_token(TokenType::Less);
            break;

        // > >= >>
        case L'>':
            if (match(L'='))      add_token(TokenType::GreaterEqual);
            else if (match(L'>')) add_token(TokenType::GreaterGreater);
            else                  add_token(TokenType::Greater);
            break;

        // & &&
        case L'&':
            add_token(match(L'&') ? TokenType::AmpAmp : TokenType::Amp);
            break;

        // | ||
        case L'|':
            add_token(match(L'|') ? TokenType::PipePipe : TokenType::Pipe);
            break;

        // / // /* */
        case L'/':
            if (match(L'/')) {
                while (peek() != L'\n' && !is_at_end()) advance();
            } else if (match(L'*')) {
                // Block comment
                while (!is_at_end()) {
                    if (peek() == L'*' && peek_next() == L'/') {
                        advance(); advance();
                        break;
                    }
                    if (advance() == L'\n') { m_line++; m_column = 1; }
                }
            } else if (match(L'=')) {
                add_token(TokenType::SlashEqual);
            } else {
                add_token(TokenType::Slash);
            }
            break;

        // Whitespace
        case L' ':
        case L'\r':
        case L'\t':
            break;
        case L'\n':
            m_line++;
            m_column = 1;
            break;

        // Strings
        case L'"':
        case L'\'':
            string(c);
            break;

        // Template literals
        case L'`':
            template_string();
            break;

        default:
            if (iswdigit(c)) {
                number();
            } else if (iswalpha(c) || c == L'_' || c == L'$') {
                identifier();
            }
            // Unknown characters silently ignored for malformed-input resilience
            break;
    }
}

void Lexer::identifier() {
    while (iswalnum(peek()) || peek() == L'_' || peek() == L'$') advance();
    std::wstring text = m_source.substr(m_start, m_current - m_start);
    auto it = g_keywords.find(text);
    TokenType type = (it != g_keywords.end()) ? it->second : TokenType::Identifier;
    add_token(type);
}

void Lexer::number() {
    // Hex literals
    if (m_source[m_start] == L'0' && (peek() == L'x' || peek() == L'X')) {
        advance(); // consume 'x'
        size_t hex_start = m_current;
        while (iswxdigit(peek())) advance();
        std::wstring hex = m_source.substr(hex_start, m_current - hex_start);
        if (hex.empty()) {
            std::wcerr << L"Lexer Error: Invalid hex literal at line " << m_line << std::endl;
            add_token(TokenType::Number, L"0");
            return;
        }
        unsigned long val = std::stoul(hex, nullptr, 16);
        add_token(TokenType::Number, std::to_wstring((double)val));
        return;
    }
    while (iswdigit(peek())) advance();
    if (peek() == L'.' && iswdigit(peek_next())) {
        advance();
        while (iswdigit(peek())) advance();
    }
    // Exponent
    if (peek() == L'e' || peek() == L'E') {
        advance();
        if (peek() == L'+' || peek() == L'-') advance();
        while (iswdigit(peek())) advance();
    }
    add_token(TokenType::Number);
}

void Lexer::string(wchar_t quote) {
    while (peek() != quote && !is_at_end()) {
        if (peek() == L'\n') { m_line++; m_column = 1; }
        if (peek() == L'\\') { advance(); } // skip escape sequence prefix
        advance();
    }
    if (is_at_end()) {
        // Report error for unterminated string
        std::wcerr << L"Lexer Error: Unterminated string at line " << m_line << L", column " << m_column << std::endl;
        // Add empty string token to allow recovery
        add_token(TokenType::String, L"");
        return;
    }
    advance(); // closing quote
    std::wstring value = m_source.substr(m_start + 1, m_current - m_start - 2);
    add_token(TokenType::String, value);
}

void Lexer::template_string() {
    std::wstring value;
    while (peek() != L'`' && !is_at_end()) {
        if (peek() == L'\n') { m_line++; m_column = 1; }
        value += advance();
    }
    if (is_at_end()) {
        // Report error for unterminated template string
        std::wcerr << L"Lexer Error: Unterminated template string at line " << m_line << std::endl;
        add_token(TokenType::TemplateString, L"");
        return;
    }
    advance(); // closing backtick
    add_token(TokenType::TemplateString, value);
}

bool Lexer::is_at_end() const { return m_current >= (int)m_source.length(); }
wchar_t Lexer::advance() { m_column++; return m_source[m_current++]; }
wchar_t Lexer::peek() const { return is_at_end() ? L'\0' : m_source[m_current]; }
wchar_t Lexer::peek_next() const {
    if (m_current + 1 >= (int)m_source.length()) return L'\0';
    return m_source[m_current + 1];
}
bool Lexer::match(wchar_t expected) {
    if (is_at_end()) return false;
    if (m_source[m_current] != expected) return false;
    m_current++;
    m_column++;
    return true;
}
void Lexer::add_token(TokenType type) {
    add_token(type, m_source.substr(m_start, m_current - m_start));
}
void Lexer::add_token(TokenType type, std::wstring lexeme) {
    m_tokens.emplace_back(type, std::move(lexeme), m_line, m_column);
}

} // namespace hjs::lexer
