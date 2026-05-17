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

class Value {
public:
    Value() : m_data(std::monostate{}) {}
    explicit Value(bool b)   : m_data(b) {}
    explicit Value(double d) : m_data(d) {}
    explicit Value(int i)    : m_data((double)i) {}
    explicit Value(std::wstring s) : m_data(std::move(s)) {}
    explicit Value(std::shared_ptr<JSObject> obj) : m_data(std::move(obj)) {}

    // Type checks
    bool is_undefined() const { return std::holds_alternative<std::monostate>(m_data); }
    bool is_null()      const { return is_undefined(); } // alias
    bool is_number()    const { return std::holds_alternative<double>(m_data); }
    bool is_boolean()   const { return std::holds_alternative<bool>(m_data); }
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

    // Convert to display string — proper JS number formatting
    std::wstring to_string() const {
        if (is_undefined()) return L"undefined";
        if (is_boolean())   return as_boolean() ? L"true" : L"false";
        if (is_null())      return L"undefined";
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
    std::variant<std::monostate, bool, double, std::wstring, std::shared_ptr<JSObject>> m_data;
};

} // namespace hjs
