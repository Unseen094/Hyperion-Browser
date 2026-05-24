#pragma once

#include "hre/css/style_engine.hpp"
#include <string>
#include <memory>
#include <variant>
#include <cmath>

namespace hre::css {

enum class CalcUnit {
    None,
    Number,
    Pixel,
    Percent,
    Em,
    Rem,
    ViewWidth,
    ViewHeight,
    Angle
};

struct CalcValue {
    double number = 0.0;
    CalcUnit unit = CalcUnit::None;
    bool valid = false;
};

class CalcEngine {
public:
    CalcEngine();

    CalcValue evaluate(const std::wstring& expression, double base_size = 16.0,
                       double viewport_width = 0.0, double viewport_height = 0.0);

    CalcValue evaluate(const std::wstring& expression,
                       const CalcValue& font_relative_base,
                       const CalcValue& viewport);

    static double convert_to_pixels(const CalcValue& val, double base_size,
                                    double viewport_width, double viewport_height);

    static bool is_valid_calc(const std::wstring& expression);

private:
    struct Token {
        enum Type { Number, Percent, Identifier, Plus, Minus, Star, Slash,
                    LParen, RParen, End };
        Type type;
        CalcValue value;
        std::wstring text;
    };

    std::vector<Token> tokenize(const std::wstring& expr);
    CalcValue parse_expression(size_t& pos);
    CalcValue parse_term(size_t& pos);
    CalcValue parse_factor(size_t& pos);
    CalcValue parse_atom(size_t& pos);
    Token peek(size_t pos) const;
    Token consume(size_t& pos);

    bool check_type_compatibility(CalcUnit left, CalcUnit right) const;
    CalcValue apply_unit_conversion(const CalcValue& val, CalcUnit target,
                                    double base_size, double vw, double vh) const;

    std::vector<Token> m_tokens;
    double m_base_size;
    double m_vw;
    double m_vh;
};

} // namespace hre::css
