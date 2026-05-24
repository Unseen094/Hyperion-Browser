#pragma once

#include <hyperion/ui/component.hpp>
#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/ui/dpi_manager.hpp>
#include <hyperion/ui/vector_icon.hpp>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>

namespace hyperion::ui {

struct bookmark_card {
    std::wstring title;
    std::wstring url;
    std::wstring favicon_path;
    D2D1_RECT_F rect;
    float hover_anim = 0.0f;
};

class modern_bookmarks_bar : public component {
public:
    modern_bookmarks_bar();
    void render(renderer& r) override;
    void handle_resize(int width, int height) override;
    void handle_mouse_down(int x, int y) override;
    void handle_mouse_move(int x, int y) override;

    void add_bookmark(const std::wstring& title, const std::wstring& url);
    void remove_bookmark(const std::wstring& url);
    bool has_bookmark(const std::wstring& url) const;
    std::vector<bookmark_card> bookmarks() const { return m_bookmarks; }

    void on_bookmark_click(std::function<void(const std::wstring&)> cb) { m_click_cb = std::move(cb); }
    void set_visible(bool v) { m_visible = v; }
    bool visible() const { return m_visible; }
    int height() const { return m_height; }

private:
    void render_bookmark_card(renderer& r, const bookmark_card& card, float x, float y, float item_h);

    std::vector<bookmark_card> m_bookmarks;
    std::function<void(const std::wstring&)> m_click_cb;
    int m_width = 0;
    int m_height = 32;
    bool m_visible = true;
    int m_hovered_index = -1;
};

} // namespace hyperion::ui
