#pragma once

#include <hjs/core/value.hpp>
#include <hjs/runtime/object.hpp>
#include <regex>
#include <string>
#include <set>

namespace hjs {

struct UnicodeSetProperty {
    std::wstring property_name;
    bool is_negative = false;
    bool is_string_prop = false;
};

class RegExp : public JSObject {
public:
    RegExp(std::wstring pattern, std::wstring flags);

    std::wstring pattern() const { return m_pattern; }
    std::wstring flags() const { return m_flags; }
    bool global() const { return m_global; }
    bool ignore_case() const { return m_ignore_case; }
    bool multiline() const { return m_multiline; }
    bool dot_all() const { return m_dot_all; }
    bool unicode() const { return m_unicode; }
    bool unicode_sets() const { return m_unicode_sets; }
    bool sticky() const { return m_sticky; }
    bool has_indices() const { return m_has_indices; }

    Value exec(const std::wstring& str);
    bool test(const std::wstring& str);
    Value match(const std::wstring& str);
    Value match_all(const std::wstring& str);
    Value replace(const std::wstring& str, const std::wstring& replacement);

    std::vector<int> search(const std::wstring& str);

    int last_index() const { return m_last_index; }
    void set_last_index(int i) { m_last_index = i; }

    static Value create(Value receiver, int argc, Value* args);

private:
    void compile_pattern();
    bool match_at(const std::wstring& str, int pos, std::vector<int>& captures);

    std::wstring m_pattern;
    std::wstring m_flags;
    bool m_global = false;
    bool m_ignore_case = false;
    bool m_multiline = false;
    bool m_dot_all = false;
    bool m_unicode = false;
    bool m_unicode_sets = false;
    bool m_sticky = false;
    bool m_has_indices = false;
    int m_last_index = 0;

    std::regex m_regex;
    std::wstring m_ecmascript_pattern;
};

} // namespace hjs
