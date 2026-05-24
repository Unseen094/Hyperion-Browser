#include <hre/css/containment.hpp>
#include <algorithm>
#include <sstream>

namespace hre::css {

contain_value contain_value::parse(const std::wstring& value) {
    contain_value result;

    if (value.empty() || value == L"none") {
        return result;
    }

    std::wistringstream iss(value);
    std::wstring token;

    while (iss >> token) {
        if (token == L"layout") result.layout = true;
        else if (token == L"paint") result.paint = true;
        else if (token == L"style") result.style = true;
        else if (token == L"size") result.size = true;
        else if (token == L"content") result.content = true;
        else if (token == L"inline-layout") result.inline_layout = true;
        else if (token == L"inline-paint") result.inline_paint = true;
    }

    return result;
}

std::wstring contain_value::to_string() const {
    if (!has_containment()) {
        return L"none";
    }

    std::wostringstream oss;
    bool first = true;

    auto add = [&](const wchar_t* name, bool enabled) {
        if (enabled) {
            if (!first) oss << L" ";
            oss << name;
            first = false;
        }
    };

    add(L"layout", layout);
    add(L"paint", paint);
    add(L"style", style);
    add(L"size", size);
    add(L"content", content);
    add(L"inline-layout", inline_layout);
    add(L"inline-paint", inline_paint);

    return oss.str();
}

contain_value parse_contain_property(const std::wstring& value) {
    return contain_value::parse(value);
}

} // namespace hre::css