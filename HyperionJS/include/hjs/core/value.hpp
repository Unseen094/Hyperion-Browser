#pragma once

#include <variant>
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace hjs {

class JSObject;

class BigInt {
public:
    BigInt() : m_value(0) {}
    explicit BigInt(int64_t v) : m_value(v) {}
    explicit BigInt(const std::wstring& s) : m_value(0) {
        std::wstring str = s;
        bool neg = false;
        if (str[0] == L'-') { neg = true; str = str.substr(1); }
        for (wchar_t c : str) { m_value = m_value * 10 + (c - L'0'); }
        if (neg) m_value = -m_value;
    }
    int64_t value() const { return m_value; }
    std::wstring to_string() const { return std::to_wstring(m_value); }
    BigInt operator+(const BigInt& o) const { return BigInt(m_value + o.m_value); }
    BigInt operator-(const BigInt& o) const { return BigInt(m_value - o.m_value); }
    BigInt operator*(const BigInt& o) const { return BigInt(m_value * o.m_value); }
    BigInt operator/(const BigInt& o) const { return BigInt(m_value / o.m_value); }
    BigInt operator%(const BigInt& o) const { return BigInt(m_value % o.m_value); }
    bool operator==(const BigInt& o) const { return m_value == o.m_value; }
    bool operator<(const BigInt& o) const { return m_value < o.m_value; }
    bool operator>(const BigInt& o) const { return m_value > o.m_value; }
private:
    int64_t m_value;
};

class Value {
public:
    Value() : m_data(std::monostate{}) {}
    explicit Value(bool b)   : m_data(b) {}
    explicit Value(double d) : m_data(d) {}
    explicit Value(int i)    : m_data((double)i) {}
    explicit Value(int64_t bi) : m_data(BigInt(bi)) {}
    explicit Value(std::wstring s) : m_data(std::move(s)) {}
    explicit Value(std::shared_ptr<JSObject> obj) : m_data(std::move(obj)) {}

    // Type checks
    bool is_undefined() const { return std::holds_alternative<std::monostate>(m_data); }
    bool is_null()      const { return is_undefined(); }
    bool is_number()    const { return std::holds_alternative<double>(m_data); }
    bool is_boolean()   const { return std::holds_alternative<bool>(m_data); }
    bool is_bigint()    const { return std::holds_alternative<BigInt>(m_data); }
    bool is_string()    const { return std::holds_alternative<std::wstring>(m_data); }
    bool is_object()    const { return std::holds_alternative<std::shared_ptr<JSObject>>(m_data); }

    // Accessors
    double as_number() const {
        if (is_number())  return std::get<double>(m_data);
        if (is_boolean()) return std::get<bool>(m_data) ? 1.0 : 0.0;
        if (is_string()) {
            try { return std::stod(std::get<std::wstring>(m_data)); }
            catch (...) { return std::numeric_limits<double>::quiet_NaN(); }
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    bool as_boolean() const {
        if (is_undefined()) return false;
        if (is_boolean())   return std::get<bool>(m_data);
        if (is_number())    return std::get<double>(m_data) != 0.0 && !std::isnan(std::get<double>(m_data));
        if (is_string())    return !std::get<std::wstring>(m_data).empty();
        if (is_object())    return true;
        return false;
    }

    const std::wstring& as_string() const {
        return std::get<std::wstring>(m_data);
    }

    std::shared_ptr<JSObject> as_object() const {
        return std::get<std::shared_ptr<JSObject>>(m_data);
    }

    BigInt as_bigint() const {
        return std::get<BigInt>(m_data);
    }

    // Convert to display string — proper JS number formatting
    std::wstring to_string() const {
        if (is_undefined()) return L"undefined";
        if (is_boolean())   return as_boolean() ? L"true" : L"false";
        if (is_null())      return L"undefined";
        if (is_bigint())    return as_bigint().to_string() + L"n";
        if (is_string())    return as_string();
        if (is_object())    return L"[object Object]";
        if (is_number()) {
            double n = std::get<double>(m_data);
            if (std::isnan(n))  return L"NaN";
            if (std::isinf(n))  return n > 0 ? L"Infinity" : L"-Infinity";
            // Integer formatting (no trailing .000000)
            if (n == std::floor(n) && std::abs(n) < 1e15) {
                std::wostringstream ss;
                ss << std::fixed << std::setprecision(0) << n;
                return ss.str();
            }
            // Floating point
            std::wostringstream ss;
            ss << n;
            return ss.str();
        }
        return L"null";
    }

private:
    std::variant<std::monostate, bool, double, BigInt, std::wstring, std::shared_ptr<JSObject>> m_data;
};

} // namespace hjs
