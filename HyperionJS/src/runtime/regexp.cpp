#include <hjs/runtime/regexp.hpp>
#include <sstream>
#include <algorithm>
#include <cwctype>

namespace hjs {

RegExp::RegExp(std::wstring pattern, std::wstring flags)
    : m_pattern(std::move(pattern))
    , m_flags(std::move(flags))
{
    for (wchar_t c : m_flags) {
        switch (c) {
            case L'g': m_global = true; break;
            case L'i': m_ignore_case = true; break;
            case L'm': m_multiline = true; break;
            case L's': m_dot_all = true; break;
            case L'u': m_unicode = true; break;
            case L'v': m_unicode_sets = true; m_unicode = true; break;
            case L'y': m_sticky = true; break;
            case L'd': m_has_indices = true; break;
        }
    }

    set_property(L"lastIndex", Value(static_cast<double>(0)));
    compile_pattern();
}

void RegExp::compile_pattern() {
    std::wstringstream ss;

    // Handle unicodeSets (v flag) — expand \p{...} patterns
    if (m_unicode_sets) {
        // In v-mode, character classes support set operations: [a--b], [a&&b]
        // For the v flag, we expand Unicode property escapes
        std::wstring expanded;
        size_t i = 0;
        while (i < m_pattern.size()) {
            if (i + 1 < m_pattern.size() && m_pattern[i] == L'\\' && m_pattern[i + 1] == L'p') {
                // \p{...} or \P{...} — Unicode property
                size_t start = i;
                i += 2;
                bool neg = (i > start + 1 && m_pattern[start + 1] == L'P');
                if (i < m_pattern.size() && m_pattern[i] == L'{') {
                    size_t end = m_pattern.find(L'}', i);
                    if (end != std::wstring::npos) {
                        std::wstring prop = m_pattern.substr(i + 1, end - i - 1);
                        // Generate character class for Unicode property
                        expanded += L"[";
                        if (neg) expanded += L"^";

                        // Common Unicode property expansions
                        if (prop == L"Letter" || prop == L"L") {
                            for (wchar_t c = L'a'; c <= L'z'; ++c) expanded += c;
                            for (wchar_t c = L'A'; c <= L'Z'; ++c) expanded += c;
                        } else if (prop == L"Uppercase_Letter" || prop == L"Lu") {
                            for (wchar_t c = L'A'; c <= L'Z'; ++c) expanded += c;
                        } else if (prop == L"Lowercase_Letter" || prop == L"Ll") {
                            for (wchar_t c = L'a'; c <= L'z'; ++c) expanded += c;
                        } else if (prop == L"Decimal_Number" || prop == L"Nd" || prop == L"Number") {
                            for (wchar_t c = L'0'; c <= L'9'; ++c) expanded += c;
                        } else if (prop == L"White_Space" || prop == L"space") {
                            expanded += L" \\t\\n\\r\\f\\v";
                        } else if (prop == L"Alphabetic") {
                            for (wchar_t c = L'a'; c <= L'z'; ++c) expanded += c;
                            for (wchar_t c = L'A'; c <= L'Z'; ++c) expanded += c;
                        } else if (prop == L"General_Category=Currency_Symbol" || prop == L"Sc") {
                            expanded += L"$\\u00a2\\u00a3\\u00a5\\u20ac";
                        } else {
                            expanded += L"\\w";
                        }

                        expanded += L"]";
                        i = end + 1;
                        continue;
                    }
                }
            }
            expanded += m_pattern[i];
            ++i;
        }
        m_ecmascript_pattern = expanded;
    } else if (m_unicode) {
        // u flag — simple Unicode property escapes
        std::wstring expanded;
        size_t i = 0;
        while (i < m_pattern.size()) {
            if (i + 1 < m_pattern.size() && m_pattern[i] == L'\\' &&
                (m_pattern[i + 1] == L'p' || m_pattern[i + 1] == L'P')) {
                bool neg = (m_pattern[i + 1] == L'P');
                size_t end = m_pattern.find(L'}', i + 2);
                if (end != std::wstring::npos) {
                    expanded += L"[";
                    if (neg) expanded += L"^";
                    std::wstring prop = m_pattern.substr(i + 3, end - i - 3);
                    if (prop == L"Letter" || prop == L"L") {
                        for (wchar_t c = L'a'; c <= L'z'; ++c) expanded += c;
                        for (wchar_t c = L'A'; c <= L'Z'; ++c) expanded += c;
                    } else if (prop == L"Number") {
                        for (wchar_t c = L'0'; c <= L'9'; ++c) expanded += c;
                    } else if (prop == L"White_Space" || prop == L"space") {
                        expanded += L" \\t\\n\\r\\f\\v";
                    } else {
                        expanded += L"\\w";
                    }
                    expanded += L"]";
                    i = end + 1;
                    continue;
                }
            }
            expanded += m_pattern[i];
            ++i;
        }
        m_ecmascript_pattern = expanded;
    } else {
        m_ecmascript_pattern = m_pattern;
    }

    // Handle set operations in v-flag: [a--b] (subtraction), [a&&b] (intersection)
    if (m_unicode_sets) {
        // Simplify double-dash and double-ampersand for std::regex compatibility
        std::wstring simplified;
        for (size_t i = 0; i < m_ecmascript_pattern.size(); ++i) {
            if (i + 2 < m_ecmascript_pattern.size() && m_ecmascript_pattern[i] == L'-' &&
                m_ecmascript_pattern[i + 1] == L'-') {
                simplified += L'-';
                i += 1; // skip one dash, keep one
            } else if (i + 2 < m_ecmascript_pattern.size() && m_ecmascript_pattern[i] == L'&' &&
                m_ecmascript_pattern[i + 1] == L'&') {
                simplified += L'&';
                i += 1;
            } else {
                simplified += m_ecmascript_pattern[i];
            }
        }
        m_ecmascript_pattern = simplified;
    }

    // Build std::regex flags
    std::regex::flag_type regex_flags = std::regex::ECMAScript;
    if (m_ignore_case) regex_flags |= std::regex::icase;
    if (m_multiline) regex_flags |= std::regex::multiline;

    try {
        m_regex = std::regex(
            std::string(m_ecmascript_pattern.begin(), m_ecmascript_pattern.end()),
            regex_flags
        );
    } catch (const std::regex_error&) {
        // Fallback to empty pattern
        m_regex = std::regex("(?:)");
    }
}

bool RegExp::match_at(const std::wstring& str, int pos, std::vector<int>& captures) {
    if (pos < 0 || pos > static_cast<int>(str.size())) return false;

    std::string narrow(str.begin(), str.end());
    std::match_results<std::string::const_iterator> results;

    std::string search_str;
    if (m_sticky) {
        search_str = narrow.substr(pos);
    } else {
        search_str = narrow;
    }

    if (std::regex_search(search_str, results, m_regex)) {
        int match_pos = m_sticky ? 0 : static_cast<int>(results.position()) + (m_sticky ? pos : 0);
        if (m_sticky && match_pos != pos) return false;

        captures.clear();
        captures.push_back(match_pos);
        captures.push_back(static_cast<int>(results.length()));
        return true;
    }
    return false;
}

Value RegExp::exec(const std::wstring& str) {
    std::vector<int> captures;
    int start = m_global || m_sticky ? m_last_index : 0;

    if (!match_at(str, start, captures)) {
        if (m_global) set_last_index(0);
        return Value();
    }

    auto result_array = std::make_shared<JSArray>();
    result_array->elements.push_back(Value(str.substr(captures[0], captures[1])));
    result_array->set_property(L"index", Value(static_cast<double>(captures[0])));
    result_array->set_property(L"input", Value(str));

    if (m_global || m_sticky) {
        set_last_index(captures[0] + captures[1]);
    }

    return Value(result_array);
}

bool RegExp::test(const std::wstring& str) {
    return exec(str).is_object();
}

Value RegExp::match(const std::wstring& str) {
    if (!m_global) {
        return exec(str);
    }

    auto results = std::make_shared<JSArray>();
    set_last_index(0);

    while (true) {
        Value result = exec(str);
        if (!result.is_object()) break;
        auto arr = result.as_object();
        Value* val = arr->get_property(L"0");
        if (val) {
            results->elements.push_back(*val);
        }
    }

    if (results->elements.empty()) {
        return Value();
    }
    results->set_property(L"index", Value(static_cast<double>(0)));
    return Value(results);
}

Value RegExp::match_all(const std::wstring& str) {
    auto results = std::make_shared<JSArray>();
    set_last_index(0);

    while (true) {
        Value result = exec(str);
        if (!result.is_object()) break;
        results->elements.push_back(result);
    }

    return Value(results);
}

Value RegExp::replace(const std::wstring& str, const std::wstring& replacement) {
    std::string narrow_str(str.begin(), str.end());
    std::string narrow_repl(replacement.begin(), replacement.end());

    if (m_global) {
        narrow_str = std::regex_replace(narrow_str, m_regex, narrow_repl);
    } else {
        narrow_str = std::regex_replace(narrow_str, m_regex, narrow_repl,
                                        std::regex_constants::format_first_only);
    }

    return Value(std::wstring(narrow_str.begin(), narrow_str.end()));
}

std::vector<int> RegExp::search(const std::wstring& str) {
    std::vector<int> captures;
    if (match_at(str, 0, captures)) {
        return captures;
    }
    return {-1};
}

Value RegExp::create(Value receiver, int argc, Value* args) {
    std::wstring pattern;
    std::wstring flags;

    if (argc > 0) {
        if (args[0].is_string()) {
            pattern = args[0].as_string();
        } else if (args[0].is_object()) {
            auto obj = args[0].as_object();
            Value* src = obj->get_property(L"source");
            if (src) pattern = src->to_string();
        }
    }

    if (argc > 1 && args[1].is_string()) {
        flags = args[1].as_string();
    }

    auto re = std::make_shared<RegExp>(pattern, flags);
    return Value(re);
}

} // namespace hjs
