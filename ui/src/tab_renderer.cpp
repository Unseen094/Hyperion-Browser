#include <hyperion/ui/tab_renderer.hpp>
#include <hyperion/ui/dwrite_helpers.hpp>
#include <hyperion/ui/toolbar.hpp>

namespace hyperion::ui {

tab_renderer::tab_renderer() = default;

void tab_renderer::render(toolbar* toolbar, renderer& r, const std::vector<tab_ui_info>& tabs,
                           float base_x, float base_y, float avail_width) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float max_tab_w = dpi.scale_px(260.0f);
    float min_tab_w = dpi.scale_px(90.0f);
    float tab_h = dpi.scale_px(36.0f);
    float corner = dpi.scale_px(10.0f);

    float tab_w = tabs.empty() ? 0 : std::max(min_tab_w, std::min(max_tab_w,
        (avail_width - dpi.scale_px(40.0f)) / (float)tabs.size()));

    m_tab_rects.clear();
    m_close_rects.clear();

    float x = base_x;
    for (int i = 0; i < (int)tabs.size(); i++) {
        D2D1_RECT_F tab_rect = D2D1::RectF(x, base_y, x + tab_w, base_y + tab_h);
        m_tab_rects.push_back(tab_rect);

        // Store close button rect
        float close_sz = dpi.scale_px(20.0f);
        float cx = tab_rect.right - close_sz - dpi.scale_px(8.0f);
        float cy = tab_rect.top + ((tab_rect.bottom - tab_rect.top) - close_sz) / 2;
        m_close_rects.push_back(D2D1::RectF(cx, cy, cx + close_sz, cy + close_sz));

        render_single_tab(r, tabs[i], tab_rect, corner, i == m_hovered_tab);
        m_last_tab_end = x + tab_w;
        x += tab_w + dpi.scale_px(1.0f);
    }

    float px = x + dpi.scale_px(4.0f);
    render_plus_button(r, px, base_y);
    m_plus_rect = {(int)px, (int)base_y, (int)(px + dpi.scale_px(32.0f)), (int)(base_y + tab_h)};
}

void tab_renderer::render_single_tab(renderer& r, const tab_ui_info& tab,
                                      const D2D1_RECT_F& rect, float corner_radius,
                                      bool is_hovered) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1::ColorF bg = tab.active ? colors.tab_active_bg : colors.tab_inactive_bg;
    float bg_alpha = tab.active ? 1.0f : (is_hovered ? 0.85f : 0.6f);
    bg.a = bg_alpha;

    ComPtr<ID2D1SolidColorBrush> bg_brush;
    ctx->CreateSolidColorBrush(bg, &bg_brush);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, corner_radius, corner_radius), bg_brush.Get());

    // Active tab: shadow glow
    if (tab.active) {
        D2D1_RECT_F shadow_r = D2D1::RectF(rect.left + dpi.scale_px(2.0f), rect.bottom,
                                             rect.right - dpi.scale_px(2.0f), rect.bottom + dpi.scale_px(4.0f));
        ComPtr<ID2D1SolidColorBrush> shadow_brush;
        ctx->CreateSolidColorBrush(colors.shadow, &shadow_brush);
        ctx->FillRectangle(shadow_r, shadow_brush.Get());

        ComPtr<ID2D1SolidColorBrush> glow_brush;
        ctx->CreateSolidColorBrush(colors.glow_accent, &glow_brush);
        D2D1_RECT_F glow_r = D2D1::RectF(rect.left + dpi.scale_px(6.0f), rect.bottom - dpi.scale_px(2.0f),
                                           rect.right - dpi.scale_px(6.0f), rect.bottom + dpi.scale_px(1.0f));
        ctx->FillRoundedRectangle(D2D1::RoundedRect(glow_r, 2.0f, 2.0f), glow_brush.Get());
    }

    // Navigation icon emoji (favicon fallback)
    float icon_size = dpi.scale_px(16.0f);
    float icon_x = rect.left + dpi.scale_px(10.0f);
    float icon_y = rect.top + (rect.bottom - rect.top - icon_size) / 2;
    D2D1_RECT_F icon_r = D2D1::RectF(icon_x, icon_y, icon_x + icon_size, icon_y + icon_size);

    if (!tab.favicon_emoji.empty()) {
        ComPtr<IDWriteTextFormat> emoji_fmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Emoji",
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &emoji_fmt);
        ComPtr<ID2D1SolidColorBrush> icon_brush;
        ctx->CreateSolidColorBrush(colors.text_secondary, &icon_brush);
        ctx->DrawText(tab.favicon_emoji.c_str(), (UINT32)tab.favicon_emoji.length(), emoji_fmt.Get(), icon_r, icon_brush.Get());
    }

    // Title text
    D2D1_RECT_F text_r = D2D1::RectF(icon_r.right + dpi.scale_px(6.0f), rect.top,
                                       rect.right - dpi.scale_px(36.0f), rect.bottom);

    ComPtr<IDWriteTextFormat> fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(12.5f), L"en-us", &fmt);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    fmt->SetTrimming(&trim, nullptr);
    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    ComPtr<ID2D1SolidColorBrush> text_brush;
    D2D1::ColorF tc = colors.text_primary;
    if (!tab.active) tc.a = 0.7f;
    ctx->CreateSolidColorBrush(tc, &text_brush);

    ctx->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ctx->DrawText(tab.title.c_str(), (UINT32)tab.title.length(), fmt.Get(), text_r, text_brush.Get());
    ctx->PopAxisAlignedClip();

    // Close button
    float close_sz = dpi.scale_px(20.0f);
    float cx = rect.right - close_sz - dpi.scale_px(8.0f);
    float cy = rect.top + ((rect.bottom - rect.top) - close_sz) / 2;
    D2D1_RECT_F close_r = D2D1::RectF(cx, cy, cx + close_sz, cy + close_sz);

    if (tab.active || is_hovered) {
        D2D1_POINT_2F cc = D2D1::Point2F((close_r.left + close_r.right) / 2, (close_r.top + close_r.bottom) / 2);
        ComPtr<ID2D1SolidColorBrush> close_hover;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.08f), &close_hover);
        ctx->FillEllipse(D2D1::Ellipse(cc, close_sz / 2, close_sz / 2), close_hover.Get());

        ComPtr<ID2D1SolidColorBrush> close_fg;
        ctx->CreateSolidColorBrush(colors.icon_inactive, &close_fg);
        float s = dpi.scale_px(5.0f);
        ctx->DrawLine(D2D1::Point2F(cc.x - s, cc.y - s), D2D1::Point2F(cc.x + s, cc.y + s), close_fg.Get(), dpi.scale_px(1.5f));
        ctx->DrawLine(D2D1::Point2F(cc.x + s, cc.y - s), D2D1::Point2F(cc.x - s, cc.y + s), close_fg.Get(), dpi.scale_px(1.5f));
    }
}

void tab_renderer::render_plus_button(renderer& r, float x, float y) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float sz = dpi.scale_px(32.0f);
    float r_y = y + dpi.scale_px(2.0f);
    D2D1_RECT_F pr = D2D1::RectF(x, r_y, x + sz, r_y + sz - dpi.scale_px(4.0f));
    D2D1_POINT_2F c = D2D1::Point2F((pr.left + pr.right) / 2, (pr.top + pr.bottom) / 2);

    ComPtr<ID2D1SolidColorBrush> bg;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0.04f), &bg);
    ctx->FillEllipse(D2D1::Ellipse(c, dpi.scale_px(14.0f), dpi.scale_px(14.0f)), bg.Get());

    ComPtr<ID2D1SolidColorBrush> fg;
    ctx->CreateSolidColorBrush(colors.accent, &fg);
    float s = dpi.scale_px(8.0f);
    ctx->DrawLine(D2D1::Point2F(c.x - s, c.y), D2D1::Point2F(c.x + s, c.y), fg.Get(), dpi.scale_px(2.0f));
    ctx->DrawLine(D2D1::Point2F(c.x, c.y - s), D2D1::Point2F(c.x, c.y + s), fg.Get(), dpi.scale_px(2.0f));
}

RECT tab_renderer::tab_rect_at(int index) const {
    if (index >= 0 && index < (int)m_tab_rects.size()) {
        auto& r = m_tab_rects[index];
        return {(int)r.left, (int)r.top, (int)r.right, (int)r.bottom};
    }
    return {};
}

RECT tab_renderer::close_rect_at(int index) const {
    if (index >= 0 && index < (int)m_close_rects.size()) {
        auto& r = m_close_rects[index];
        return {(int)r.left, (int)r.top, (int)r.right, (int)r.bottom};
    }
    return {};
}

void tab_renderer::handle_mouse_down(int x, int y) {
    // Check plus button first
    if (x >= m_plus_rect.left && x <= m_plus_rect.right &&
        y >= m_plus_rect.top && y <= m_plus_rect.bottom) {
        if (m_new_tab_cb) m_new_tab_cb();
        return;
    }

    // Check close buttons (iterate in reverse so topmost tabs are checked first)
    for (int i = (int)m_close_rects.size() - 1; i >= 0; i--) {
        auto& r = m_close_rects[i];
        if (x >= (int)r.left && x <= (int)r.right &&
            y >= (int)r.top && y <= (int)r.bottom) {
            if (m_tab_close_cb) m_tab_close_cb(i);
            return;
        }
    }

    // Check tab clicks
    for (int i = (int)m_tab_rects.size() - 1; i >= 0; i--) {
        auto& r = m_tab_rects[i];
        if (x >= (int)r.left && x <= (int)r.right &&
            y >= (int)r.top && y <= (int)r.bottom) {
            if (m_tab_click_cb) m_tab_click_cb(i);
            return;
        }
    }
}

void tab_renderer::handle_mouse_move(int x, int y) {
    m_hovered_tab = -1;
    for (int i = (int)m_tab_rects.size() - 1; i >= 0; i--) {
        auto& r = m_tab_rects[i];
        if (x >= (int)r.left && x <= (int)r.right &&
            y >= (int)r.top && y <= (int)r.bottom) {
            m_hovered_tab = i;
            return;
        }
    }
}

} // namespace hyperion::ui
