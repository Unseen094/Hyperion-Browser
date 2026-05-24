#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <windows.h>
#include <wrl/client.h>

#include <hre/render/render_command.hpp>
#include <hre/layout/layout_engine.hpp>
#include <hre/script/script_engine.hpp>
#pragma push_macro("DrawText")
#undef DrawText
#include <d2d1_3.h>
#pragma pop_macro("DrawText")

namespace hyperion::ui { class renderer; }
namespace hre::script { class script_engine; }

namespace hyperion::browser {

class tab {
public:
    explicit tab(const std::wstring& initial_url);
    ~tab();

    const std::wstring& url() const { return m_url; }
    const std::wstring& title() const { return m_title; }
    void set_title(const std::wstring& t) { m_title = t; }

    void navigate(const std::wstring& url);
    void back();
    void forward();
    void reload();

    void set_visible(bool visible);
    bool visible() const { return m_visible; }

    bool can_go_back() const { return m_history_index > 0; }
    bool can_go_forward() const { return m_history_index + 1 < m_history.size(); }
    double progress() const { return m_progress; }

    void initialize_webview(HWND parent_hwnd);
    void resize_webview(const RECT& bounds);

    void render_native(ui::renderer& r, float y_offset = 108.0f);
    bool is_native() const { return m_use_native; }
    void handle_mouse_wheel(float delta);
    void handle_mouse_down(int x, int y);
    void handle_mouse_move(int x, int y);
    void handle_key_down(UINT vk, bool ctrl, bool alt, bool shift);
    void handle_char(wchar_t c);
    bool is_transitioning() const { return m_transition_active; }

    std::function<void(const std::wstring&)> on_title_changed;

    struct TransitionAnim {
        double start_time = 0;
        double duration = 0.3;
        std::wstring timing_function = L"ease";
        uint32_t old_color = 0;
        uint32_t new_color = 0;
        float old_x = 0, old_y = 0, old_w = 0, old_h = 0;
        float new_x = 0, new_y = 0, new_w = 0, new_h = 0;
        bool active = false;
    };

private:
    void render_page_in_process(const std::wstring& url);
    void render_error_page(const std::wstring& message);
    void build_click_targets(const hre::layout::LayoutNode& node, float y_offset);
    void clear_page_state();
    void regenerate_commands();
    void invalidate_element_style(hre::dom::element* el, bool is_hovered, bool is_focused);
    hre::dom::element* find_form_parent(hre::dom::element* el);
    void start_transition(uint32_t old_color, uint32_t new_color,
                          float old_x, float old_y, float old_w, float old_h,
                          float new_x, float new_y, float new_w, float new_h,
                          double duration);
    void update_transitions();
    static double now_seconds();

    std::wstring m_url;
    std::wstring m_title;
    HWND m_parent_hwnd = nullptr;

    std::vector<std::wstring> m_history;
    size_t m_history_index = 0;

    double m_progress = 0.0;

    bool m_use_native = false;
    bool m_visible = true;
    float m_scroll_top = 0.0f;
    RECT m_bounds = {0, 0, 1920, 1080};

    std::mutex m_commands_mutex;
    std::vector<hre::render::render_command> m_cached_commands;

    std::unique_ptr<hre::script::script_engine> m_script_engine;
    std::shared_ptr<hre::layout::LayoutNode> m_layout_root;
    std::vector<hre::script::ClickTarget> m_click_targets;
    float m_last_y_offset = 0;

    hre::dom::element* m_focused_element = nullptr;
    hre::dom::element* m_hovered_element = nullptr;

    std::unique_ptr<hre::layout::LayoutEngine> m_layout_engine;
    float m_y_offset = 0;

    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap1>> m_image_cache;

    TransitionAnim m_transition;
    std::vector<hre::render::render_command> m_old_commands;
    bool m_transition_active = false;
};

} // namespace hyperion::browser
