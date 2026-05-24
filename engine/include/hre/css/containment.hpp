#pragma once

#include <string>
#include <cstdint>

namespace hre::css {

enum class contain_type : uint32_t {
    NONE = 0,
    LAYOUT = 1 << 0,
    PAINT = 1 << 1,
    STYLE = 1 << 2,
    SIZE = 1 << 3,
    CONTENT = 1 << 4,
    INLINE_LAYOUT = 1 << 5,
    INLINE_PAINT = 1 << 6
};

struct contain_value {
    bool layout = false;
    bool paint = false;
    bool style = false;
    bool size = false;
    bool content = false;
    bool inline_layout = false;
    bool inline_paint = false;

    bool has_containment() const {
        return layout || paint || style || size || content || inline_layout || inline_paint;
    }

    bool has_layout() const { return layout || content; }
    bool has_paint() const { return paint || content; }

    static contain_value parse(const std::wstring& value);
    std::wstring to_string() const;
};

contain_value parse_contain_property(const std::wstring& value);

} // namespace hre::css