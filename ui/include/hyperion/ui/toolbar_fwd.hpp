#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace hyperion::ui {

struct tab_ui_info {
    std::wstring title;
    bool active;
    RECT rect;
    RECT close_rect;
    std::wstring favicon_emoji; // emoji fallback for site icon
};

class toolbar;

} // namespace hyperion::ui
