#pragma once

#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/toolbar.hpp>
#include <vector>
#include <string>
#include <functional>

namespace hyperion::ui {

struct bookmark_info {
    std::wstring title;
    std::wstring url;
    RECT rect;
};

class bookmarks_bar : public component {
public:
    bookmarks_bar();
    void render(renderer& r) override;
    void handle_resize(int width, int height) override;
    void handle_mouse_down(int x, int y) override;

    void add_bookmark(const std::wstring& title, const std::wstring& url);
    void on_bookmark_click(std::function<void(const std::wstring&)> cb) { m_click_cb = std::move(cb); }

    int height() const { return m_height; }

private:
    int m_width = 0;
    int m_height = 28;
    std::vector<bookmark_info> m_bookmarks;
    std::function<void(const std::wstring&)> m_click_cb;
};

} // namespace hyperion::ui
