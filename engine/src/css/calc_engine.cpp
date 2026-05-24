#include "hre/css/calc_engine.hpp"
#include <sstream>
#include <cctype>
#include <cwctype>
#include <algorithm>

namespace hre::css {

CalcEngine::CalcEngine()
    : m_base_size(16.0)
    , m_vw(0.0)
    , m_vh(0.0)
{
}

std::vector<CalcEngine::Token> CalcEngine::tokenize(const std::wstring& expr) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < expr.size()) {
        if (std::iswspace(expr[i])) { i++; continue; }

        if (std::iswdigit(expr[i]) || expr[i] == L'.') {
            std::wstring num_str;
            while (i < expr.size() && (std::iswdigit(expr[i]) || expr[i] == L'.')) {
                num_str += expr[i++];
            }

            Token t;
            t.type = Token::Number;
            t.value.number = std::stod(num_str);

            if (i + 1 < expr.size()) {
                if (expr[i] == L'p' && expr[i+1] == L'x') {
                    t.value.unit = CalcUnit::Pixel;
                    i += 2;
                } else if (expr[i] == L'e' && expr[i+1] == L'm') {
                    t.value.unit = CalcUnit::Em;
                    i += 2;
                } else if (i + 2 < expr.size() && expr[i] == L'r' && expr[i+1] == L'e' && expr[i+2] == L'm') {
                    t.value.unit = CalcUnit::Rem;
                    i += 3;
                } else if (expr[i] == L'%') {
                    t.value.unit = CalcUnit::Percent;
                    i++;
                } else if (expr[i] == L'v' && i + 1 < expr.size()) {
                    if (expr[i+1] == L'w') { t.value.unit = CalcUnit::ViewWidth; i += 2; }
                    else if (expr[i+1] == L'h') { t.value.unit = CalcUnit::ViewHeight; i += 2; }
                } else if (expr[i] == L'd' && i + 2 < expr.size()
                           && expr[i+1] == L'e' && expr[i+2] == L'g') {
                    t.value.unit = CalcUnit::Angle;
                    i += 3;
                } else if (expr[i] == L'r' && i + 1 < expr.size() && expr[i+1] == L'a' && i + 2 < expr.size() && expr[i+2] == L'd') {
                    t.value.unit = CalcUnit::Angle;
                    i += 3;
                }
            }

            t.value.valid = true;
            tokens.push_back(t);
            continue;
        }

        if (expr[i] == L'+') { tokens.push_back({Token::Plus}); i++; continue; }
        if (expr[i] == L'-') { tokens.push_back({Token::Minus}); i++; continue; }
        if (expr[i] == L'*') { tokens.push_back({Token::Star}); i++; continue; }
        if (expr[i] == L'/') { tokens.push_back({Token::Slash}); i++; continue; }
        if (expr[i] == L'(') { tokens.push_back({Token::LParen}); i++; continue; }
        if (expr[i] == L')') { tokens.push_back({Token::RParen}); i++; continue; }

        i++;
    }

    tokens.push_back({Token::End});
    return tokens;
}

CalcEngine::Token CalcEngine::peek(size_t pos) const {
    if (pos < m_tokens.size()) return m_tokens[pos];
    return {Token::End};
}

CalcEngine::Token CalcEngine::consume(size_t& pos) {
    if (pos < m_tokens.size()) return m_tokens[pos++];
    return {Token::End};
}

CalcValue CalcEngine::parse_expression(size_t& pos) {
    CalcValue left = parse_term(pos);
    while (pos < m_tokens.size()) {
        Token op = peek(pos);
        if (op.type == Token::Plus) {
            consume(pos);
            CalcValue right = parse_term(pos);
            if (!check_type_compatibility(left.unit, right.unit)) {
                left.valid = false;
                return left;
            }
            left.number = left.number + right.number;
        } else if (op.type == Token::Minus) {
            consume(pos);
            CalcValue right = parse_term(pos);
            if (!check_type_compatibility(left.unit, right.unit)) {
                left.valid = false;
                return left;
            }
            left.number = left.number - right.number;
        } else {
            break;
        }
    }
    return left;
}

CalcValue CalcEngine::parse_term(size_t& pos) {
    CalcValue left = parse_factor(pos);
    while (pos < m_tokens.size()) {
        Token op = peek(pos);
        if (op.type == Token::Star) {
            consume(pos);
            CalcValue right = parse_factor(pos);
            if (left.unit != CalcUnit::None && right.unit != CalcUnit::None) {
                if (left.unit == CalcUnit::Number) {
                    left.number = left.number * right.number;
                    left.unit = right.unit;
                } else if (right.unit == CalcUnit::Number) {
                    left.number = left.number * right.number;
                } else {
                    left.valid = false;
                    return left;
                }
            } else {
                left.number = left.number * right.number;
            }
        } else if (op.type == Token::Slash) {
            consume(pos);
            CalcValue right = parse_factor(pos);
            if (std::abs(right.number) < 1e-10) {
                left.valid = false;
                return left;
            }
            if (right.unit != CalcUnit::None && right.unit != CalcUnit::Number) {
                left.valid = false;
                return left;
            }
            left.number = left.number / right.number;
        } else {
            break;
        }
    }
    return left;
}

CalcValue CalcEngine::parse_factor(size_t& pos) {
    Token t = peek(pos);
    if (t.type == Token::Plus) {
        consume(pos);
        return parse_factor(pos);
    }
    if (t.type == Token::Minus) {
        consume(pos);
        CalcValue val = parse_factor(pos);
        val.number = -val.number;
        return val;
    }
    return parse_atom(pos);
}

CalcValue CalcEngine::parse_atom(size_t& pos) {
    Token t = peek(pos);
    if (t.type == Token::Number) {
        consume(pos);
        return t.value;
    }
    if (t.type == Token::LParen) {
        consume(pos);
        CalcValue val = parse_expression(pos);
        t = consume(pos);
        if (t.type != Token::RParen) val.valid = false;
        return val;
    }
    return {0, CalcUnit::None, false};
}

bool CalcEngine::check_type_compatibility(CalcUnit left, CalcUnit right) const {
    if (left == CalcUnit::Number && right == CalcUnit::Number) return true;
    if (left == CalcUnit::None && right == CalcUnit::None) return true;

    auto is_length = [](CalcUnit u) {
        return u == CalcUnit::Pixel || u == CalcUnit::Percent
            || u == CalcUnit::Em || u == CalcUnit::Rem
            || u == CalcUnit::ViewWidth || u == CalcUnit::ViewHeight;
    };

    auto is_angle = [](CalcUnit u) { return u == CalcUnit::Angle; };

    if (is_length(left) && is_length(right)) return true;
    if (is_angle(left) && is_angle(right)) return true;
    if (left == CalcUnit::None) return true;
    if (right == CalcUnit::None) return true;
    if (left == CalcUnit::Number) return true;
    if (right == CalcUnit::Number) return true;

    return false;
}

CalcValue CalcEngine::evaluate(const std::wstring& expression, double base_size,
                                double viewport_width, double viewport_height)
{
    m_base_size = base_size;
    m_vw = viewport_width;
    m_vh = viewport_height;

    std::wstring expr = expression;
    size_t calc_pos = expr.find(L"calc(");
    if (calc_pos != std::wstring::npos) {
        size_t end = expr.find_last_of(L")");
        if (end != std::wstring::npos && end > calc_pos + 5) {
            expr = expr.substr(calc_pos + 5, end - calc_pos - 5);
        }
    }

    m_tokens = tokenize(expr);
    size_t pos = 0;
    CalcValue result = parse_expression(pos);
    return result;
}

CalcValue CalcEngine::evaluate(const std::wstring& expression,
                                const CalcValue& font_relative_base,
                                const CalcValue& viewport)
{
    double base = font_relative_base.valid ? font_relative_base.number : 16.0;
    double vw = viewport.valid ? viewport.number : 0.0;
    double vh = viewport.valid ? viewport.number : 0.0;
    return evaluate(expression, base, vw, vh);
}

double CalcEngine::convert_to_pixels(const CalcValue& val, double base_size,
                                      double viewport_width, double viewport_height)
{
    switch (val.unit) {
    case CalcUnit::Pixel:
    case CalcUnit::Number:
        return val.number;
    case CalcUnit::Percent:
        return val.number / 100.0 * base_size;
    case CalcUnit::Em:
        return val.number * base_size;
    case CalcUnit::Rem:
        return val.number * 16.0;
    case CalcUnit::ViewWidth:
        return val.number / 100.0 * viewport_width;
    case CalcUnit::ViewHeight:
        return val.number / 100.0 * viewport_height;
    case CalcUnit::Angle:
        return val.number;
    default:
        return val.number;
    }
}

CalcValue CalcEngine::apply_unit_conversion(const CalcValue& val, CalcUnit target,
                                              double base_size, double vw, double vh) const
{
    if (val.unit == target) return val;
    double px = convert_to_pixels(val, base_size, vw, vh);
    CalcValue result;
    result.number = px;
    result.unit = target;
    result.valid = true;
    return result;
}

bool CalcEngine::is_valid_calc(const std::wstring& expression) {
    CalcEngine engine;
    size_t calc_pos = expression.find(L"calc(");
    if (calc_pos == std::wstring::npos) return false;

    size_t end = expression.find_last_of(L")");
    if (end == std::wstring::npos || end <= calc_pos + 5) return false;

    std::wstring inner = expression.substr(calc_pos + 5, end - calc_pos - 5);
    auto tokens = engine.tokenize(inner);
    if (tokens.empty()) return false;

    size_t depth = 0;
    bool expect_value = true;
    for (const auto& tok : tokens) {
        switch (tok.type) {
        case Token::Number:
            expect_value = false;
            break;
        case Token::LParen:
            depth++;
            break;
        case Token::RParen:
            if (depth == 0) return false;
            depth--;
            expect_value = false;
            break;
        case Token::Plus:
        case Token::Minus:
        case Token::Star:
        case Token::Slash:
            if (expect_value) return false;
            expect_value = true;
            break;
        case Token::End:
            break;
        default:
            break;
        }
    }
    return depth == 0 && !expect_value;
}

} // namespace hre::css
