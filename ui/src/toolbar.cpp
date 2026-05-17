#include <hyperion/ui/toolbar.hpp>

namespace hyperion::ui {

toolbar::toolbar() {}

void toolbar::render(renderer& r) {
    auto* context = r.context();
    if (!context) return;

    // --- DAY MODE (LIGHT THEME) COLORS ---
    auto color_bg = D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.0f);        // Modern light gray
    auto color_tab_active = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);  // White active tab
    auto color_tab_inactive = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);
    auto color_omni_bg = D2D1::ColorF(0.92f, 0.92f, 0.94f, 1.0f);   // Slightly inset omnibox
    auto color_text = D2D1::ColorF(0.2f, 0.2f, 0.23f, 1.0f);       // Dark gray text
    auto color_accent = D2D1::ColorF(0.1f, 0.5f, 0.9f, 1.0f);      // Blue accent

    // Draw main toolbar background
    D2D1_RECT_F rect = D2D1::RectF(0, 0, (float)m_width, (float)m_height);
    ComPtr<ID2D1SolidColorBrush> brush;
    context->CreateSolidColorBrush(color_bg, &brush);
    context->FillRectangle(rect, brush.Get());

    // Bottom border for the toolbar
    ComPtr<ID2D1SolidColorBrush> border_brush;
    context->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.85f, 0.87f, 1.0f), &border_brush);
    context->DrawLine(D2D1::Point2F(0, (float)m_height - 0.5f), D2D1::Point2F((float)m_width, (float)m_height - 0.5f), border_brush.Get(), 1.0f);

    // Draw Tab Strip
    float max_tab_width = 240.0f;
    float min_tab_width = 100.0f;
    float tab_strip_padding = 8.0f;
    float available_width = (float)m_width - (tab_strip_padding * 2.0f) - 40.0f; // 40 for plus button
    
    float tab_width = available_width / (float)(std::max)((size_t)1, m_tabs.size());
    if (tab_width > max_tab_width) tab_width = max_tab_width;
    if (tab_width < min_tab_width) tab_width = min_tab_width;

    float tab_height = 36.0f;
    float tab_x = tab_strip_padding;
    float tab_y = 6.0f;

    ComPtr<IDWriteTextFormat> tab_text_format;
    r.write_factory()->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5f, L"en-us", &tab_text_format);
    tab_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    tab_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        D2D1_RECT_F tab_rect = D2D1::RectF(tab_x, tab_y, tab_x + tab_width, tab_y + tab_height);
        
        ComPtr<ID2D1SolidColorBrush> tab_brush;
        context->CreateSolidColorBrush(m_tabs[i].active ? color_tab_active : color_tab_inactive, &tab_brush);
        
        // Modern Tab Shape (rounded top corners)
        D2D1_ROUNDED_RECT rounded_tab = D2D1::RoundedRect(tab_rect, 10.0f, 10.0f);
        context->FillRoundedRectangle(rounded_tab, tab_brush.Get());
        
        if (m_tabs[i].active) {
            // Subtle top highlight for active tab
            D2D1_RECT_F highlight = D2D1::RectF(tab_rect.left, tab_rect.top, tab_rect.right, tab_rect.top + 2.0f);
            ComPtr<ID2D1SolidColorBrush> acc_brush;
            context->CreateSolidColorBrush(color_accent, &acc_brush);
            context->FillRectangle(highlight, acc_brush.Get());
        }

        ComPtr<ID2D1SolidColorBrush> text_brush;
        D2D1::ColorF tab_text_color = color_text;
        if (!m_tabs[i].active) tab_text_color.a = 0.6f;
        context->CreateSolidColorBrush(tab_text_color, &text_brush);
        
        D2D1_RECT_F text_rect = tab_rect;
        text_rect.left += 15.0f;
        text_rect.right -= 35.0f; 

        DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        tab_text_format->SetTrimming(&trimming, nullptr);

        context->PushAxisAlignedClip(tab_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        context->DrawText(m_tabs[i].title.c_str(), (UINT32)m_tabs[i].title.length(), 
            tab_text_format.Get(), text_rect, text_brush.Get());
        
        // Draw Close Button (X)
        D2D1_RECT_F close_btn_rect = D2D1::RectF(tab_rect.right - 28.0f, tab_rect.top + 8.0f, tab_rect.right - 8.0f, tab_rect.top + 28.0f);
        if (m_tabs[i].active) {
            ComPtr<ID2D1SolidColorBrush> close_hover_brush;
            context->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.05f), &close_hover_brush);
            context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(close_btn_rect.left + 10, close_btn_rect.top + 10), 10, 10), close_hover_brush.Get());
        }
        
        tab_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        context->DrawText(L"×", 1, tab_text_format.Get(), close_btn_rect, text_brush.Get());
        tab_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); 

        context->PopAxisAlignedClip();

        m_tabs[i].rect = { (int)tab_x, (int)tab_y, (int)(tab_x + tab_width), (int)(tab_y + tab_height) };
        m_tabs[i].close_rect = { (int)close_btn_rect.left, (int)close_btn_rect.top, (int)close_btn_rect.right, (int)close_btn_rect.bottom };

        tab_x += tab_width + 1.0f;
    }

    // New Tab Button (+)
    D2D1_RECT_F plus_rect = D2D1::RectF(tab_x + 4.0f, tab_y + 4.0f, tab_x + 32.0f, tab_y + 32.0f);
    ComPtr<ID2D1SolidColorBrush> plus_brush;
    context->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.06f), &plus_brush);
    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(plus_rect.left + 14, plus_rect.top + 14), 14, 14), plus_brush.Get());
    
    ComPtr<ID2D1SolidColorBrush> accent_brush;
    context->CreateSolidColorBrush(color_accent, &accent_brush);
    tab_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    context->DrawText(L"+", 1, tab_text_format.Get(), plus_rect, accent_brush.Get());

    // --- NAVIGATION TOOLBAR ---
    float btn_size = 32.0f;
    float btn_y = tab_y + tab_height + 6.0f;
    float btn_x = 12.0f;

    auto draw_nav_btn = [&](const wchar_t* symbol, RECT& out_rect, bool enabled) {
        D2D1_RECT_F btn_rect = D2D1::RectF(btn_x, btn_y, btn_x + btn_size, btn_y + btn_size);
        
        ComPtr<ID2D1SolidColorBrush> text_brush;
        D2D1::ColorF btn_color = color_text;
        btn_color.a = enabled ? 0.9f : 0.25f;
        context->CreateSolidColorBrush(btn_color, &text_brush);
        
        tab_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        context->DrawText(symbol, (UINT32)wcslen(symbol), tab_text_format.Get(), btn_rect, text_brush.Get());
        
        out_rect = { (int)btn_rect.left, (int)btn_rect.top, (int)btn_rect.right, (int)btn_rect.bottom };
        btn_x += btn_size + 8.0f;
    };

    draw_nav_btn(L"←", m_back_rect, m_can_go_back);
    draw_nav_btn(L"→", m_forward_rect, m_can_go_forward);
    draw_nav_btn(L"⟳", m_refresh_rect, true);
    draw_nav_btn(L"⌂", m_home_rect, true);

    // Draw Omnibox
    float omni_y = btn_y + 2.0f;
    float omni_height = 30.0f;
    float omni_x = btn_x + 10.0f;
    D2D1_RECT_F omni_rect = D2D1::RectF(omni_x, omni_y, (float)m_width - 15.0f, omni_y + omni_height);
    
    ComPtr<ID2D1SolidColorBrush> omni_brush;
    context->CreateSolidColorBrush(color_omni_bg, &omni_brush);
    context->FillRoundedRectangle(D2D1::RoundedRect(omni_rect, 15.0f, 15.0f), omni_brush.Get());

    // Inset border for Omnibox
    ComPtr<ID2D1SolidColorBrush> omni_border;
    context->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.08f), &omni_border);
    context->DrawRoundedRectangle(D2D1::RoundedRect(omni_rect, 15.0f, 15.0f), omni_border.Get(), 1.0f);

    // Address text
    if (!m_address.empty()) {
        ComPtr<IDWriteTextFormat> omni_format;
        r.write_factory()->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.5f, L"en-us", &omni_format);
        omni_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        
        DWRITE_TRIMMING omni_trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        omni_format->SetTrimming(&omni_trimming, nullptr);
        omni_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        D2D1_RECT_F text_rect = omni_rect;
        text_rect.left += 20.0f;
        text_rect.right -= 40.0f;
        
        ComPtr<ID2D1SolidColorBrush> address_brush;
        D2D1::ColorF addr_color = color_text;
        addr_color.a = 0.85f;
        context->CreateSolidColorBrush(addr_color, &address_brush);

        context->PushAxisAlignedClip(omni_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        context->DrawText(m_address.c_str(), (UINT32)m_address.length(), 
            omni_format.Get(), text_rect, address_brush.Get());
        context->PopAxisAlignedClip();

        // Draw Star Icon
        D2D1_RECT_F star_rect = D2D1::RectF(omni_rect.right - 35.0f, omni_rect.top, omni_rect.right - 5.0f, omni_rect.bottom);
        ComPtr<ID2D1SolidColorBrush> star_brush;
        context->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.75f, 0.0f, 0.9f), &star_brush); 
        
        tab_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        context->DrawText(L"★", 1, tab_text_format.Get(), star_rect, star_brush.Get());
        
        m_star_rect = { (int)star_rect.left, (int)star_rect.top, (int)star_rect.right, (int)star_rect.bottom };
    }

    // Progress Bar
    if (m_progress > 0.0 && m_progress < 1.0) {
        D2D1_RECT_F progress_rect = D2D1::RectF(0, (float)m_height - 2.5f, (float)m_width * (float)m_progress, (float)m_height);
        context->FillRectangle(progress_rect, accent_brush.Get());
    }
}

void toolbar::handle_resize(int width, int height) {
    (void)height;
    m_width = width;
}

void toolbar::handle_char(wchar_t c) {
    if (c == L'\b') { // Backspace
        if (!m_address.empty()) {
            m_address.pop_back();
        }
    } else if (c >= 32) { // Printable characters
        m_address += c;
    }
}

void toolbar::handle_mouse_down(int x, int y) {
    // Check tab clicks
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        // Check close button first
        if (x >= m_tabs[i].close_rect.left && x <= m_tabs[i].close_rect.right &&
            y >= m_tabs[i].close_rect.top && y <= m_tabs[i].close_rect.bottom) {
            if (m_tab_close_cb) m_tab_close_cb(i);
            return;
        }

        // Check tab body
        if (x >= m_tabs[i].rect.left && x <= m_tabs[i].rect.right &&
            y >= m_tabs[i].rect.top && y <= m_tabs[i].rect.bottom) {
            if (m_tab_click_cb) m_tab_click_cb(i);
            return;
        }
    }

    // Check Navigation Buttons
    auto check_btn = [&](const RECT& r, const std::function<void()>& cb) {
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            if (cb) cb();
            return true;
        }
        return false;
    };

    if (check_btn(m_back_rect, m_back_cb)) return;
    if (check_btn(m_forward_rect, m_forward_cb)) return;
    if (check_btn(m_refresh_rect, m_refresh_cb)) return;
    if (check_btn(m_home_rect, m_home_cb)) return;

    // Check Star (Bookmark)
    if (x >= m_star_rect.left && x <= m_star_rect.right &&
        y >= m_star_rect.top && y <= m_star_rect.bottom) {
        if (m_add_bookmark_cb) m_add_bookmark_cb(L"New Bookmark", m_address);
        return;
    }

    // Check New Tab click
    float max_tab_width = 240.0f;
    float min_tab_width = 100.0f;
    float tab_strip_padding = 8.0f;
    float available_width = (float)m_width - (tab_strip_padding * 2.0f) - 40.0f;
    float tab_width = available_width / (float)(std::max)((size_t)1, m_tabs.size());
    if (tab_width > max_tab_width) tab_width = max_tab_width;
    if (tab_width < min_tab_width) tab_width = min_tab_width;

    float plus_x = tab_strip_padding + (m_tabs.size() * (tab_width + 1.0f));
    if (x >= plus_x && x <= plus_x + 32.0f &&
        y >= 10 && y <= 42) {
        if (m_new_tab_click_cb) m_new_tab_click_cb();
    }
}

} // namespace hyperion::ui
