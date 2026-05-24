#pragma once

#include <hyperion/ui/renderer.hpp>

namespace hyperion::ui {

class component {
public:
    virtual ~component() = default;
    virtual void render(renderer& r) = 0;
    virtual void handle_mouse_move(int x, int y) { (void)x; (void)y; }
    virtual void handle_mouse_down(int x, int y) { (void)x; (void)y; }
    virtual void handle_char(wchar_t c) { (void)c; }
    virtual void handle_key_down(UINT vk) { (void)vk; }
    virtual void handle_resize(int width, int height) { (void)width; (void)height; }
};

} // namespace hyperion::ui
