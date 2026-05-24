#include <hyperion/ui/toolbar.hpp>
#include <algorithm>

namespace hyperion::ui {

toolbar::toolbar()
    : m_omnibox(std::make_unique<omnibox_edit>())
    , m_suggestions(std::make_unique<suggestion_popup>())
    , m_tab_renderer(std::make_unique<tab_renderer>())
    , m_new_tab(std::make_unique<new_tab_dashboard>())
    , m_ai_sidebar(std::make_unique<ai_sidebar>())
    , m_window_chrome(&window_chrome_manager::instance())
{}

toolbar::~toolbar() = default;

void toolbar::initialize(HWND hwnd, renderer& r) {
    dpi_manager::instance().initialize(hwnd);
    theme_manager::instance().set_hwnd(hwnd);
    m_icons.initialize(r);
    m_glass.initialize(r);
    m_bookmarks_bar.handle_resize(m_width, m_height);

    auto& tm = theme_manager::instance();
    tm.apply_mica_backdrop();
    tm.apply_rounded_corners();
    m_window_chrome->apply(hwnd);

    m_omnibox->on_text_changed([this](const std::wstring& text) {
        if (m_show_dashboard && !text.empty()) {
            hide_dashboard();
        }
    });

    m_omnibox->on_enter([this](const std::wstring& text) {
        m_suggestions->set_visible(false);
        hide_dashboard();
        if (m_omnibox_enter_cb) m_omnibox_enter_cb(text);
    });

    m_suggestions->on_suggestion_click([this](const suggestion_entry& entry) {
        m_omnibox->set_text(entry.text);
        m_suggestions->set_visible(false);
        hide_dashboard();
        if (!entry.url.empty()) {
            if (m_omnibox_enter_cb) m_omnibox_enter_cb(entry.url);
        }
    });

    m_new_tab->initialize(m_weather_api_key);
    m_new_tab->set_navigate_callback([this](const std::wstring& url) {
        if (m_omnibox_enter_cb) {
            m_omnibox->set_text(url);
            m_omnibox_enter_cb(url);
        }
        hide_dashboard();
    });

    m_bookmarks_bar.on_bookmark_click([this](const std::wstring& url) {
        if (!url.empty() && m_omnibox_enter_cb) {
            m_omnibox->set_text(url);
            m_omnibox_enter_cb(url);
        }
        hide_dashboard();
    });

    // Wire tab renderer callbacks
    m_tab_renderer->on_tab_click([this](int index) {
        if (m_tab_click_cb) m_tab_click_cb((size_t)index);
    });

    m_tab_renderer->on_tab_close_click([this](int index) {
        if (m_tab_close_cb) m_tab_close_cb((size_t)index);
    });

    m_tab_renderer->on_new_tab_click([this]() {
        if (m_new_tab_click_cb) m_new_tab_click_cb();
    });

    m_ai_sidebar->initialize_ai(
        "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",
        "meta/llama-3.1-8b-instruct"
    );

    m_initialized = true;
}

void toolbar::hide_dashboard() {
    if (m_show_dashboard) {
        m_show_dashboard = false;
        m_new_tab->set_visible(false);
    }
}

void toolbar::set_address(const std::wstring& address) {
    m_omnibox->set_text(address);
    hide_dashboard();
}

void toolbar::set_navigation_state(bool can_back, bool can_forward) {
    m_can_go_back = can_back;
    m_can_go_forward = can_forward;
}

void toolbar::set_progress(double progress) {
    m_progress = progress;
}

void toolbar::set_theme(theme_type type) {
    theme_manager::instance().set_theme(type);
    m_window_chrome->update_theme(type == THEME_DARK || type == THEME_HIGH_CONTRAST);
}

void toolbar::animate_tab_close(int index) {
    // Placeholder: trigger a redraw to remove the tab from the strip
    (void)index;
}

void toolbar::animate_tab_open() {
    // Placeholder: trigger a redraw to show the new tab
}

void toolbar::show_new_tab_dashboard() {
    m_show_dashboard = true;
    m_new_tab->set_visible(true);
    m_new_tab->set_rect(D2D1::RectF(
        0.0f,
        (float)total_ui_height(),
        (float)m_width,
        (float)m_window_height
    ));
    m_omnibox->set_text(L"");
    m_suggestions->set_visible(false);
}

void toolbar::toggle_ai_sidebar() {
    m_ai_sidebar->toggle();
    m_sidebar_open = m_ai_sidebar->is_open();
}

bool toolbar::is_point_interactive(int x, int y) const {
    // Returns false for dead zones (window drag areas), true for interactive controls
    if (y < 0 || y > total_ui_height()) return false;

    auto check_rect = [&](const RECT& r) -> bool {
        return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
    };

    // Tab strip (full width, top area)
    auto& dpi = dpi_manager::instance();
    float tab_strip_top = dpi.scale_px(4.0f);
    float tab_strip_bot = tab_strip_top + dpi.scale_px(36.0f);
    if (y >= (int)tab_strip_top && y <= (int)tab_strip_bot) return true;

    // Nav buttons
    if (check_rect(m_back_rect)) return true;
    if (check_rect(m_forward_rect)) return true;
    if (check_rect(m_refresh_rect)) return true;
    if (check_rect(m_home_rect)) return true;
    if (check_rect(m_star_rect)) return true;

    // Omnibox area
    if (check_rect(m_omni_rect)) return true;

    // AI sidebar toggle (right edge)
    float toggle_left = (float)m_width - dpi.scale_px(44.0f);
    float toggle_right = (float)m_width - dpi.scale_px(4.0f);
    if (x >= toggle_left && x <= toggle_right && y >= 0 && y <= dpi.scale_px(36.0f)) return true;

    // Check bookmark bar (translated y)
    int bm_y = y - m_height;
    if (bm_y >= 0 && bm_y <= (int)m_bookmarks_bar.height()) {
        for (const auto& bm : m_bookmarks_bar.bookmarks()) {
            RECT r = {(int)bm.rect.left, (int)(bm.rect.top + m_height),
                      (int)bm.rect.right, (int)(bm.rect.bottom + m_height)};
            if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) return true;
        }
    }

    return false;
}

void toolbar::render(renderer& r) {
    auto* ctx = r.context();
    if (!ctx || !m_initialized) return;
    auto& dpi = dpi_manager::instance();
    auto& colors = theme_manager::instance().colors();

    dpi.update_dpi();

    // -- 1. Render toolbar background (top m_height pixels) --
    D2D1_RECT_F toolbar_bg = D2D1::RectF(0, 0, (float)m_width, (float)m_height);
    ComPtr<ID2D1SolidColorBrush> bg_brush;
    ctx->CreateSolidColorBrush(colors.background, &bg_brush);
    ctx->FillRectangle(toolbar_bg, bg_brush.Get());

    // Bottom divider of toolbar
    ComPtr<ID2D1SolidColorBrush> div_brush;
    ctx->CreateSolidColorBrush(colors.divider, &div_brush);
    ctx->DrawLine(
        D2D1::Point2F(0, (float)m_height - 0.5f),
        D2D1::Point2F((float)m_width, (float)m_height - 0.5f),
        div_brush.Get(), 1.0f
    );

    // -- 2. Tab strip --
    render_tab_strip(r);

    // -- 3. Navigation buttons --
    render_nav_buttons(r);

    // -- 4. Omnibox area --
    render_omnibox_area(r);

    // -- 5. Security badge --
    render_security_badge(r);

    // -- 6. Progress bar --
    if (m_progress > 0.0 && m_progress < 1.0) {
        ComPtr<ID2D1SolidColorBrush> prog_brush;
        ctx->CreateSolidColorBrush(colors.accent, &prog_brush);
        D2D1_RECT_F pr = D2D1::RectF(
            0, (float)m_height - dpi.scale_px(2.5f),
            (float)m_width * (float)m_progress, (float)m_height
        );
        ctx->FillRectangle(pr, prog_brush.Get());
    }

    // -- 7. Bookmarks bar (below toolbar, rendered in its own coordinate space) --
    ctx->SetTransform(D2D1::Matrix3x2F::Translation(0, (float)m_height));
    m_bookmarks_bar.render(r);
    ctx->SetTransform(D2D1::Matrix3x2F::Identity());

    // -- 8. New Tab Dashboard (fills content area below toolbar + bookmarks) --
    if (m_show_dashboard && m_new_tab->visible()) {
        // Update dashboard rect in case window was resized
        m_new_tab->set_rect(D2D1::RectF(
            0.0f,
            (float)total_ui_height(),
            (float)m_width,
            (float)m_window_height
        ));
        m_new_tab->render(r);
    }

    // -- 9. AI Sidebar (overlays everything on the right edge) --
    float sidebar_anim = m_ai_sidebar->animation_progress();
    if (m_sidebar_open && sidebar_anim < 0.99f) {
        sidebar_anim = std::min(sidebar_anim + 0.05f, 1.0f);
    } else if (!m_sidebar_open && sidebar_anim > 0.01f) {
        sidebar_anim = std::max(sidebar_anim - 0.05f, 0.0f);
    }
    m_ai_sidebar->set_animation_progress(sidebar_anim);
    m_ai_sidebar->render(r);

    animation_controller::instance().tick();
}

void toolbar::render_tab_strip(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& dpi = dpi_manager::instance();
    auto& colors = theme_manager::instance().colors();

    float pad = dpi.scale_px(8.0f);
    float tab_y = dpi.scale_px(4.0f);
    float avail_w = (float)m_width - (pad * 2.0f) - dpi.scale_px(40.0f);

    D2D1_RECT_F strip_bg = D2D1::RectF(0, 0, (float)m_width, tab_y + dpi.scale_px(40.0f));
    ComPtr<ID2D1SolidColorBrush> strip_brush;
    ctx->CreateSolidColorBrush(colors.surface_variant, &strip_brush);
    ctx->FillRectangle(strip_bg, strip_brush.Get());

    ComPtr<ID2D1SolidColorBrush> div;
    ctx->CreateSolidColorBrush(colors.divider, &div);
    ctx->DrawLine(
        D2D1::Point2F(0, strip_bg.bottom),
        D2D1::Point2F((float)m_width, strip_bg.bottom),
        div.Get(), dpi.scale_px(0.5f)
    );

    m_tab_renderer->render(nullptr, r, m_tabs, pad, tab_y, avail_w);
}

void toolbar::render_nav_buttons(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& dpi = dpi_manager::instance();
    auto& colors = theme_manager::instance().colors();

    float nav_y = dpi.scale_px(46.0f);
    float btn_sz = dpi.scale_px(32.0f);
    float x = dpi.scale_px(10.0f);

    auto draw_btn = [&](RECT& out_rect, const wchar_t* icon_name, bool enabled, float x_pos, float y_pos) {
        D2D1_RECT_F br = D2D1::RectF(x_pos, y_pos, x_pos + btn_sz, y_pos + btn_sz);
        D2D1::ColorF ic = enabled ? colors.icon_active : colors.icon_inactive;
        m_icons.render_icon(icon_name, ctx, br, ic, enabled ? 1.0f : 0.35f);
        out_rect = {(int)br.left, (int)br.top, (int)br.right, (int)br.bottom};
    };

    draw_btn(m_back_rect, L"back", m_can_go_back, x, nav_y);
    x += btn_sz + dpi.scale_px(4.0f);
    draw_btn(m_forward_rect, L"forward", m_can_go_forward, x, nav_y);
    x += btn_sz + dpi.scale_px(4.0f);
    draw_btn(m_refresh_rect, L"refresh", true, x, nav_y);
    x += btn_sz + dpi.scale_px(4.0f);
    draw_btn(m_home_rect, L"home", true, x, nav_y);
}

void toolbar::render_omnibox_area(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& dpi = dpi_manager::instance();
    auto& colors = theme_manager::instance().colors();

    float nav_y = dpi.scale_px(46.0f);
    float omni_y = nav_y + dpi.scale_px(1.0f);
    float omni_h = dpi.scale_px(32.0f);

    float btn_end = m_home_rect.right + dpi.scale_px(8.0f);
    float omni_l = btn_end + dpi.scale_px(8.0f);
    float omni_r = (float)m_width - dpi.scale_px(12.0f);

    D2D1_RECT_F omni_rect = D2D1::RectF(omni_l, omni_y, omni_r, omni_y + omni_h);

    m_omnibox->set_omnibox_rect(omni_rect);
    m_omnibox->render(r);

    m_omni_rect = {
        (int)omni_rect.left, (int)omni_rect.top,
        (int)omni_rect.right, (int)omni_rect.bottom
    };

    if (m_suggestions->is_visible()) {
        m_suggestions->set_anchor_rect(omni_rect);
        m_suggestions->render(r);
    }

    D2D1_RECT_F star_rect = D2D1::RectF(
        omni_rect.right - dpi.scale_px(36.0f),
        omni_rect.top + dpi.scale_px(2.0f),
        omni_rect.right - dpi.scale_px(4.0f),
        omni_rect.bottom - dpi.scale_px(2.0f)
    );
    bool is_bookmarked = m_bookmarks_bar.has_bookmark(m_omnibox->text());
    m_icons.render_star(ctx, star_rect, D2D1::ColorF(1.0f, 0.75f, 0.0f, 0.9f), is_bookmarked);
    m_star_rect = {
        (int)star_rect.left, (int)star_rect.top,
        (int)star_rect.right, (int)star_rect.bottom
    };
}

void toolbar::render_security_badge(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& dpi = dpi_manager::instance();

    float nav_y = dpi.scale_px(46.0f);
    float badge_center_x = dpi.scale_px(8.0f) + dpi.scale_px(8.0f);
    float badge_center_y = nav_y + dpi.scale_px(16.0f);

    if (m_omnibox->text().find(L"https://") == 0) {
        ComPtr<ID2D1SolidColorBrush> secure_brush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.63f, 0.35f, 1.0f), &secure_brush);
        ctx->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(badge_center_x, badge_center_y), dpi.scale_px(6.0f), dpi.scale_px(6.0f)),
            secure_brush.Get());
    } else if (m_omnibox->text().find(L"http://") == 0) {
        ComPtr<ID2D1SolidColorBrush> insecure_brush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.7f, 0.7f, 1.0f), &insecure_brush);
        ctx->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(badge_center_x, badge_center_y), dpi.scale_px(6.0f), dpi.scale_px(6.0f)),
            insecure_brush.Get());
    }
}

void toolbar::handle_resize(int width, int height) {
    m_width = width;
    m_window_height = height;
    m_height = 100;

    m_bookmarks_bar.handle_resize(width, m_height);
    m_new_tab->handle_resize(width, height);
    m_ai_sidebar->handle_resize(width, height);

    if (m_show_dashboard) {
        m_new_tab->set_rect(D2D1::RectF(
            0.0f,
            (float)total_ui_height(),
            (float)m_width,
            (float)m_window_height
        ));
    }
}

void toolbar::handle_char(wchar_t c) {
    if (m_ai_sidebar->is_open()) {
        m_ai_sidebar->handle_char(c);
        return;
    }
    if (m_show_dashboard && c >= 32) {
        hide_dashboard();
    }
    m_omnibox->handle_char(c);
}

void toolbar::handle_key_down(UINT vk) {
    if (m_ai_sidebar->is_open()) {
        m_ai_sidebar->handle_key_down(vk);
    }
}

void toolbar::handle_mouse_move(int x, int y) {
    float ui_y = (float)total_ui_height();

    // Tab strip hover
    m_tab_renderer->handle_mouse_move(x, y);

    if (m_show_dashboard && y >= ui_y) {
        m_new_tab->handle_mouse_move(x, y);
    }
    if (y < m_height) {
        m_omnibox->handle_mouse_move(x, y);
    }
    m_suggestions->handle_mouse_move(x, y);
    m_ai_sidebar->handle_mouse_move(x, y);

    // Bookmark bar in its translated space
    int bm_y = y - m_height;
    if (bm_y >= 0 && bm_y <= (int)m_bookmarks_bar.height()) {
        m_bookmarks_bar.handle_mouse_move(x, bm_y);
    }
}

void toolbar::handle_mouse_down(int x, int y) {
    auto& dpi = dpi_manager::instance();

    // AI sidebar toggle button (right edge of screen)
    if (x >= m_width - dpi.scale_px(44.0f) && x <= m_width - dpi.scale_px(4.0f) &&
        y >= 0 && y <= dpi.scale_px(36.0f)) {
        toggle_ai_sidebar();
        return;
    }

    // Tab strip clicks
    float tab_strip_top = dpi.scale_px(4.0f);
    float tab_strip_bot = tab_strip_top + dpi.scale_px(36.0f);
    if (y >= (int)tab_strip_top && y <= (int)tab_strip_bot) {
        m_tab_renderer->handle_mouse_down(x, y);
        return;
    }

    // Nav buttons
    auto check = [&](const RECT& r, const std::function<void()>& cb) -> bool {
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            if (cb) cb();
            return true;
        }
        return false;
    };

    if (check(m_back_rect, m_back_cb)) return;
    if (check(m_forward_rect, m_forward_cb)) return;
    if (check(m_refresh_rect, m_refresh_cb)) return;
    if (check(m_home_rect, m_home_cb)) return;
    if (check(m_star_rect, [this]() {
        if (m_add_bookmark_cb) m_add_bookmark_cb(L"Hyperion Bookmark", m_omnibox->text());
    })) return;

    // Omnibox right-side icons (extensions, profile, settings, AI, downloads)
    // These are handled by omnibox::handle_mouse_down
    if (x >= m_omni_rect.left && x <= m_omni_rect.right &&
        y >= m_omni_rect.top && y <= m_omni_rect.bottom) {
        hide_dashboard();
        m_omnibox->handle_mouse_down(x, y);
        return;
    }

    // Suggestions
    if (y < m_height) {
        m_suggestions->handle_mouse_down(x, y);
    }

    // Bookmark bar (translated space)
    int bm_y = y - m_height;
    if (bm_y >= 0 && bm_y <= (int)m_bookmarks_bar.height()) {
        m_bookmarks_bar.handle_mouse_down(x, bm_y);
        return;
    }

    // Dashboard (content area below toolbar + bookmarks)
    if (m_show_dashboard && y >= total_ui_height()) {
        m_new_tab->handle_mouse_down(x, y);
    }

    // AI sidebar
    m_ai_sidebar->handle_mouse_down(x, y);
}

} // namespace hyperion::browser
