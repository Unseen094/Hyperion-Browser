#include <hyperion/ui/omnibox.hpp>
#include <hyperion/ui/dwrite_helpers.hpp>
#include <hyperion/platform/logging.hpp>
#include <cmath>

namespace hyperion::ui {

// === omnibox_edit ===

omnibox_edit::omnibox_edit() : m_cursor_blink_start(std::chrono::steady_clock::now()) {}

void omnibox_edit::set_text(const std::wstring& t) {
    m_text = t;
    m_cursor_pos = (int)t.length();
}

void omnibox_edit::render(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    render_pill_bg(r);
    render_lock_icon(r);
    render_text(r);
    render_right_icons(r);
}

void omnibox_edit::render_pill_bg(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F omni = dpi.scale_rect(m_omni_rect);
    float radius = (omni.bottom - omni.top) / 2;
    auto rounded = D2D1::RoundedRect(omni, radius, radius);

    // Shadow layer
    D2D1_RECT_F shad_r = D2D1::RectF(omni.left + dpi.scale_px(2.0f), omni.top + dpi.scale_px(2.0f),
                                       omni.right + dpi.scale_px(2.0f), omni.bottom + dpi.scale_px(3.0f));
    ComPtr<ID2D1SolidColorBrush> shadow_brush;
    ctx->CreateSolidColorBrush(colors.shadow, &shadow_brush);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(shad_r, radius, radius), shadow_brush.Get());

    // Pill background
    ComPtr<ID2D1SolidColorBrush> bg_brush;
    ctx->CreateSolidColorBrush(colors.omnibox_pill_bg, &bg_brush);
    ctx->FillRoundedRectangle(rounded, bg_brush.Get());

    // Border
    ComPtr<ID2D1SolidColorBrush> border_brush;
    D2D1::ColorF bc = m_focused ? colors.accent : colors.omnibox_pill_border;
    ctx->CreateSolidColorBrush(bc, &border_brush);
    ctx->DrawRoundedRectangle(rounded, border_brush.Get(), m_focused ? 1.8f : 1.0f);

    // Inner glow when focused
    if (m_focused) {
        ComPtr<ID2D1SolidColorBrush> glow_brush;
        ctx->CreateSolidColorBrush(colors.glow_accent, &glow_brush);
        D2D1_RECT_F glow_r = D2D1::RectF(omni.left + dpi.scale_px(3.0f), omni.top + dpi.scale_px(3.0f),
                                           omni.right - dpi.scale_px(3.0f), omni.bottom - dpi.scale_px(3.0f));
        ctx->DrawRoundedRectangle(D2D1::RoundedRect(glow_r, radius - 2, radius - 2), glow_brush.Get(), dpi.scale_px(1.0f));
    }
}

void omnibox_edit::render_left_icons(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F omni = dpi.scale_rect(m_omni_rect);
    float icon_sz = dpi.scale_px(18.0f);
    float y = omni.top + (omni.bottom - omni.top - icon_sz) / 2;
    float x = omni.left + dpi.scale_px(6.0f);

    // Back, Forward, Refresh, Home as thin monochrome icons
    struct { const wchar_t* label; } icons[] = {{L"\u2190"}, {L"\u2192"}, {L"\u21BB"}, {L"\u2302"}};
    for (int i = 0; i < 4; i++) {
        D2D1_RECT_F ir = D2D1::RectF(x, y, x + icon_sz, y + icon_sz);
        ComPtr<IDWriteTextFormat> fmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Symbol",
            DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &fmt);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        ComPtr<ID2D1SolidColorBrush> ib;
        ctx->CreateSolidColorBrush(colors.icon_active, &ib);
        ctx->DrawText(icons[i].label, 1, fmt.Get(), ir, ib.Get());
        x += icon_sz + dpi.scale_px(2.0f);
    }
}

void omnibox_edit::render_lock_icon(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F omni = dpi.scale_rect(m_omni_rect);
    float icon_sz = dpi.scale_px(14.0f);
    float y = omni.top + (omni.bottom - omni.top - icon_sz) / 2;
    float x = omni.left + dpi.scale_px(82.0f);
    D2D1_RECT_F lr = D2D1::RectF(x, y, x + icon_sz, y + icon_sz);

    ComPtr<ID2D1SolidColorBrush> lock_brush;
    D2D1::ColorF lc = m_is_secure ? D2D1::ColorF(0.18f, 0.63f, 0.35f, 1.0f) : colors.icon_inactive;
    ctx->CreateSolidColorBrush(lc, &lock_brush);

    if (m_is_secure) {
        float cx = (lr.left + lr.right) / 2, cy = (lr.top + lr.bottom) / 2;
        float r2 = icon_sz / 2 - 1;
        // Lock body
        ctx->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - r2 * 0.6f, cy, cx + r2 * 0.6f, cy + r2 * 0.9f), 1.5f, 1.5f), lock_brush.Get(), 1.5f);
        // Lock shackle (semi-circle using path geometry)
        ComPtr<ID2D1PathGeometry> path;
        ComPtr<ID2D1GeometrySink> sink;
        if (SUCCEEDED(r.d2d_factory()->CreatePathGeometry(&path)) &&
            SUCCEEDED(path->Open(&sink))) {
            sink->SetFillMode(D2D1_FILL_MODE_WINDING);
            sink->BeginFigure(D2D1::Point2F(cx - r2 * 0.3f, cy), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(cx + r2 * 0.3f, cy), D2D1::SizeF(r2 * 0.3f, r2 * 0.3f), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            ctx->DrawGeometry(path.Get(), lock_brush.Get(), 1.5f);
        }
    } else {
        ComPtr<IDWriteTextFormat> fmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Symbol",
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(12.0f), L"en-us", &fmt);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        ctx->DrawText(L"\u26D2", 1, fmt.Get(), lr, lock_brush.Get());
    }
}

void omnibox_edit::render_text(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F omni = dpi.scale_rect(m_omni_rect);
    float text_left = omni.left + dpi.scale_px(12.0f);
    float text_right = omni.right - dpi.scale_px(160.0f);
    D2D1_RECT_F text_rect = D2D1::RectF(text_left, omni.top, text_right, omni.bottom);

    ComPtr<IDWriteTextFormat> fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(13.5f), L"en-us", &fmt);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    fmt->SetTrimming(&trim, nullptr);
    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    if (!m_text.empty()) {
        ComPtr<IDWriteTextLayout> layout;
        r.write_factory()->CreateTextLayout(m_text.c_str(), (UINT32)m_text.length(),
            fmt.Get(), text_rect.right - text_rect.left, text_rect.bottom - text_rect.top, &layout);
        ComPtr<ID2D1SolidColorBrush> text_brush;
        ctx->CreateSolidColorBrush(colors.text_primary, &text_brush);
        ctx->PushAxisAlignedClip(omni, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        if (layout) {
            ctx->DrawTextLayout(D2D1::Point2F(text_rect.left, text_rect.top), layout.Get(), text_brush.Get());
            if (m_focused) render_cursor_from_layout(r, layout.Get(), text_rect, omni);
        } else {
            ctx->DrawText(m_text.c_str(), (UINT32)m_text.length(), fmt.Get(), text_rect, text_brush.Get());
            if (m_focused) render_cursor(r, text_rect, omni);
        }
        ctx->PopAxisAlignedClip();
    } else {
        ComPtr<ID2D1SolidColorBrush> placeholder_brush;
        ctx->CreateSolidColorBrush(colors.text_secondary, &placeholder_brush);
        ctx->PushAxisAlignedClip(omni, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        ctx->DrawText(m_placeholder.c_str(), (UINT32)m_placeholder.length(), fmt.Get(), text_rect, placeholder_brush.Get());
        ctx->PopAxisAlignedClip();
    }
}

void omnibox_edit::render_cursor_from_layout(renderer& r, IDWriteTextLayout* layout, const D2D1_RECT_F& text_rect, const D2D1_RECT_F& omni) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_cursor_blink_start).count();
    m_cursor_visible = (elapsed % 1000) < 500;
    if (!m_cursor_visible) return;

    auto* ctx = r.context();
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float cursor_x = text_rect.left;
    if (layout && m_cursor_pos > 0) {
        DWRITE_HIT_TEST_METRICS metrics;
        float hit_x = 0, hit_y = 0;
        layout->HitTestTextPosition((UINT32)m_cursor_pos, false, &hit_x, &hit_y, &metrics);
        cursor_x = text_rect.left + hit_x;
    } else {
        cursor_x = text_rect.left + m_cursor_pos * dpi.scale_px(7.5f);
    }
    float max_x = omni.right - dpi.scale_px(160.0f);
    if (cursor_x > max_x) cursor_x = max_x;

    ComPtr<ID2D1SolidColorBrush> cursor_brush;
    ctx->CreateSolidColorBrush(colors.text_primary, &cursor_brush);
    ctx->DrawLine(
        D2D1::Point2F(cursor_x, omni.top + dpi.scale_px(5.0f)),
        D2D1::Point2F(cursor_x, omni.bottom - dpi.scale_px(5.0f)),
        cursor_brush.Get(), dpi.scale_px(1.5f));
}

void omnibox_edit::render_cursor(renderer& r, const D2D1_RECT_F& text_rect, const D2D1_RECT_F& omni) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_cursor_blink_start).count();
    m_cursor_visible = (elapsed % 1000) < 500;
    if (!m_cursor_visible) return;

    auto* ctx = r.context();
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float cursor_x = text_rect.left + m_cursor_pos * dpi.scale_px(7.5f);
    float max_x = omni.right - dpi.scale_px(160.0f);
    if (cursor_x > max_x) cursor_x = max_x;

    ComPtr<ID2D1SolidColorBrush> cursor_brush;
    ctx->CreateSolidColorBrush(colors.text_primary, &cursor_brush);
    ctx->DrawLine(
        D2D1::Point2F(cursor_x, omni.top + dpi.scale_px(5.0f)),
        D2D1::Point2F(cursor_x, omni.bottom - dpi.scale_px(5.0f)),
        cursor_brush.Get(), dpi.scale_px(1.5f));
}

void omnibox_edit::render_star(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F omni = dpi.scale_rect(m_omni_rect);
    float sz = dpi.scale_px(16.0f);
    float y = omni.top + (omni.bottom - omni.top - sz) / 2;
    float x = omni.right - dpi.scale_px(156.0f);
    D2D1_RECT_F sr = D2D1::RectF(x, y, x + sz, y + sz);

    ComPtr<IDWriteTextFormat> fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Symbol",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &fmt);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    ComPtr<ID2D1SolidColorBrush> sb;
    ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.75f, 0.0f, 0.9f), &sb);
    ctx->DrawText(L"\u2605", 1, fmt.Get(), sr, sb.Get());
}

void omnibox_edit::render_right_icons(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F omni = dpi.scale_rect(m_omni_rect);
    float sz = dpi.scale_px(18.0f);
    float y = omni.top + (omni.bottom - omni.top - sz) / 2;
    float x = omni.right - dpi.scale_px(110.0f);

    struct { const wchar_t* label; } icons[] = {{L"\u2699"}, {L"\U0001F464"}, {L"\u2B50"}, {L"\U0001F4E5"}};

    for (int i = 0; i < 4; i++) {
        D2D1_RECT_F ir = D2D1::RectF(x, y, x + sz, y + sz);
        ComPtr<IDWriteTextFormat> fmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Symbol",
            DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &fmt);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        ComPtr<ID2D1SolidColorBrush> ib;
        ctx->CreateSolidColorBrush(colors.icon_active, &ib);
        ctx->DrawText(icons[i].label, 1, fmt.Get(), ir, ib.Get());
        x += sz + dpi.scale_px(8.0f);
    }
}

void omnibox_edit::handle_char(wchar_t c) {
    if (c == L'\b') {
        if (!m_text.empty() && m_cursor_pos > 0) {
            m_text.erase(m_cursor_pos - 1, 1);
            m_cursor_pos--;
            if (m_text_changed_cb) m_text_changed_cb(m_text);
        }
    } else if (c == L'\r' || c == L'\n') {
        if (m_enter_cb) m_enter_cb(m_text);
    } else if (c >= 32) {
        m_text.insert(m_cursor_pos, 1, c);
        m_cursor_pos++;
        if (m_text_changed_cb) m_text_changed_cb(m_text);
    }
    m_cursor_blink_start = std::chrono::steady_clock::now();
    m_cursor_visible = true;
}

void omnibox_edit::handle_mouse_down(int x, int y) {
    // Check right-side icon clicks first
    if (hit_extensions(x, y)) { if (m_ext_cb) m_ext_cb(); return; }
    if (hit_profile(x, y)) { if (m_profile_cb) m_profile_cb(); return; }
    if (hit_settings(x, y)) { if (m_settings_cb) m_settings_cb(); return; }
    if (hit_ai_shortcut(x, y)) { if (m_ai_cb) m_ai_cb(); return; }
    if (hit_downloads(x, y)) { if (m_dl_cb) m_dl_cb(); return; }

    m_focused = true;

    auto& dpi = dpi_manager::instance();
    float text_left = m_omni_rect.left + dpi.scale_px(12.0f);
    int char_pos = (int)((x - text_left) / dpi.scale_px(7.5f) + 0.5f);
    m_cursor_pos = std::max(0, std::min(char_pos, (int)m_text.length()));
    m_cursor_blink_start = std::chrono::steady_clock::now();
    m_cursor_visible = true;
}

void omnibox_edit::handle_resize(int width, int height) {
    (void)width; (void)height;
}

bool omnibox_edit::hit_extensions(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float sz = dpi.scale_px(18.0f);
    float icon_start = m_omni_rect.right - dpi.scale_px(110.0f);
    RECT r = {(int)icon_start, (int)m_omni_rect.top, (int)(icon_start + sz), (int)m_omni_rect.bottom};
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

bool omnibox_edit::hit_profile(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float sz = dpi.scale_px(18.0f);
    float icon_start = m_omni_rect.right - dpi.scale_px(110.0f) + sz + dpi.scale_px(8.0f);
    RECT r = {(int)icon_start, (int)m_omni_rect.top, (int)(icon_start + sz), (int)m_omni_rect.bottom};
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

bool omnibox_edit::hit_settings(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float sz = dpi.scale_px(18.0f);
    float icon_start = m_omni_rect.right - dpi.scale_px(110.0f) + 2.0f * (sz + dpi.scale_px(8.0f));
    RECT r = {(int)icon_start, (int)m_omni_rect.top, (int)(icon_start + sz), (int)m_omni_rect.bottom};
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

bool omnibox_edit::hit_ai_shortcut(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float sz = dpi.scale_px(18.0f);
    float icon_start = m_omni_rect.right - dpi.scale_px(110.0f) + 3.0f * (sz + dpi.scale_px(8.0f));
    RECT r = {(int)icon_start, (int)m_omni_rect.top, (int)(icon_start + sz), (int)m_omni_rect.bottom};
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

bool omnibox_edit::hit_downloads(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float sz = dpi.scale_px(18.0f);
    float icon_start = m_omni_rect.right - dpi.scale_px(110.0f) + 4.0f * (sz + dpi.scale_px(8.0f));
    RECT r = {(int)icon_start, (int)m_omni_rect.top, (int)(icon_start + sz), (int)m_omni_rect.bottom};
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

// === suggestion_popup ===

suggestion_popup::suggestion_popup() = default;

void suggestion_popup::set_suggestions(const std::vector<suggestion_entry>& suggestions) {
    m_suggestions = suggestions;
    m_visible = !suggestions.empty();
    m_hovered_index = -1;
}

void suggestion_popup::render(renderer& r) {
    if (!m_visible || m_suggestions.empty()) return;
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float item_h = dpi.scale_px(38.0f);
    float popup_h = (float)m_suggestions.size() * item_h + dpi.scale_px(8.0f);
    float popup_y = m_anchor_rect.bottom + dpi.scale_px(4.0f);
    D2D1_RECT_F popup_rect = D2D1::RectF(m_anchor_rect.left, popup_y, m_anchor_rect.right, popup_y + popup_h);

    D2D1_RECT_F shadow_r = D2D1::RectF(popup_rect.left + dpi.scale_px(3.0f), popup_rect.top + dpi.scale_px(3.0f),
                                         popup_rect.right + dpi.scale_px(3.0f), popup_rect.bottom + dpi.scale_px(3.0f));
    ComPtr<ID2D1SolidColorBrush> shadow_brush;
    ctx->CreateSolidColorBrush(colors.shadow, &shadow_brush);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(shadow_r, dpi.scale_px(14.0f), dpi.scale_px(14.0f)), shadow_brush.Get());

    ComPtr<ID2D1SolidColorBrush> bg_brush;
    ctx->CreateSolidColorBrush(colors.surface, &bg_brush);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(popup_rect, dpi.scale_px(14.0f), dpi.scale_px(14.0f)), bg_brush.Get());

    ComPtr<ID2D1SolidColorBrush> border_brush;
    ctx->CreateSolidColorBrush(colors.divider, &border_brush);
    ctx->DrawRoundedRectangle(D2D1::RoundedRect(popup_rect, dpi.scale_px(14.0f), dpi.scale_px(14.0f)), border_brush.Get(), 1.0f);

    ComPtr<IDWriteTextFormat> fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(13.0f), L"en-us", &fmt);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    for (int i = 0; i < (int)m_suggestions.size(); i++) {
        D2D1_RECT_F item_r = D2D1::RectF(
            popup_rect.left + dpi.scale_px(4.0f),
            popup_rect.top + dpi.scale_px(4.0f) + i * item_h,
            popup_rect.right - dpi.scale_px(4.0f),
            popup_rect.top + dpi.scale_px(4.0f) + (i + 1) * item_h);

        if (i == m_hovered_index) {
            ComPtr<ID2D1SolidColorBrush> hover_brush;
            ctx->CreateSolidColorBrush(colors.hover_overlay, &hover_brush);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(item_r, dpi.scale_px(8.0f), dpi.scale_px(8.0f)), hover_brush.Get());
        }

        D2D1_RECT_F tr = item_r;
        tr.left += dpi.scale_px(12.0f);
        ComPtr<ID2D1SolidColorBrush> text_brush;
        ctx->CreateSolidColorBrush(colors.text_primary, &text_brush);
        ctx->DrawText(m_suggestions[i].text.c_str(), (UINT32)m_suggestions[i].text.length(),
                      fmt.Get(), tr, text_brush.Get());
    }
}

void suggestion_popup::handle_mouse_move(int x, int y) {
    if (!m_visible) return;
    auto& dpi = dpi_manager::instance();
    float item_h = dpi.scale_px(38.0f);
    float popup_y = m_anchor_rect.bottom + dpi.scale_px(4.0f);
    int idx = (int)((y - popup_y - dpi.scale_px(4.0f)) / item_h);
    m_hovered_index = (idx >= 0 && idx < (int)m_suggestions.size()) ? idx : -1;
}

void suggestion_popup::handle_mouse_down(int x, int y) {
    if (!m_visible) return;
    auto& dpi = dpi_manager::instance();
    float item_h = dpi.scale_px(38.0f);
    float popup_y = m_anchor_rect.bottom + dpi.scale_px(4.0f);
    int idx = (int)((y - popup_y - dpi.scale_px(4.0f)) / item_h);
    if (idx >= 0 && idx < (int)m_suggestions.size()) {
        if (m_click_cb) m_click_cb(m_suggestions[idx]);
    }
}

void suggestion_popup::handle_resize(int width, int height) {
    (void)width; (void)height;
}

} // namespace hyperion::ui
