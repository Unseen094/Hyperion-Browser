#include <hyperion/ui/new_tab_dashboard.hpp>
#include <hyperion/ui/dwrite_helpers.hpp>
#include <hyperion/platform/logging.hpp>
#include <d2d1_3.h>
#include <cmath>

namespace hyperion::ui {

new_tab_dashboard::new_tab_dashboard()
    : m_weather(std::make_unique<weather_widget>()) {
    add_default_speed_dials();
}

new_tab_dashboard::~new_tab_dashboard() = default;

void new_tab_dashboard::initialize(const std::wstring& weather_api_key) {
    if (!weather_api_key.empty()) {
        m_weather->initialize(weather_api_key);
    }
}

void new_tab_dashboard::add_default_speed_dials() {
    m_speed_dials = {
        {L"Google",        L"https://google.com",        L""},
        {L"YouTube",       L"https://youtube.com",       L""},
        {L"GitHub",        L"https://github.com",        L""},
        {L"Reddit",        L"https://reddit.com",        L""},
        {L"X / Twitter",   L"https://x.com",             L""},
        {L"Wikipedia",     L"https://wikipedia.org",     L""},
    };
}

void new_tab_dashboard::render(renderer& r) {
    if (!m_visible) return;
    auto* ctx = r.context();
    if (!ctx) return;

    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    // --- Gradient Mesh Background ---
    render_background(r);

    // --- Hyperion Logo ---
    render_logo(r);

    // --- Search Bar ---
    render_search_bar(r);

    // --- Speed Dial Cards ---
    render_speed_dial(r);

    // --- Weather Widget ---
    float weather_w = dpi.scale_px(220.0f);
    float weather_h = dpi.scale_px(72.0f);
    D2D1_RECT_F weather_rect = D2D1::RectF(
        m_rect.right - weather_w - dpi.scale_px(24.0f),
        m_rect.top + dpi.scale_px(24.0f),
        m_rect.right - dpi.scale_px(24.0f),
        m_rect.top + dpi.scale_px(24.0f) + weather_h
    );
    m_weather->set_rect(weather_rect);
    m_weather->render(r);
}

void new_tab_dashboard::render_background(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    // Gradient mesh with 4+ stops
    ComPtr<ID2D1GradientStopCollection> stops;
    D2D1_GRADIENT_STOP stop_array[5] = {
        {0.00f, colors.bg_mesh_0},
        {0.25f, colors.bg_mesh_1},
        {0.50f, colors.bg_mesh_2},
        {0.75f, colors.bg_mesh_3},
        {1.00f, colors.bg_mesh_4},
    };
    ctx->CreateGradientStopCollection(stop_array, 5, &stops);

    ComPtr<ID2D1LinearGradientBrush> grad;
    ctx->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(m_rect.left, m_rect.top),
            D2D1::Point2F(m_rect.right, m_rect.bottom)),
        stops.Get(), &grad);
    ctx->FillRectangle(m_rect, grad.Get());

    // Subtle radial glow in center
    float cx = (m_rect.left + m_rect.right) / 2.0f;
    float cy = m_rect.top + dpi.scale_px(200.0f);
    float radius = dpi.scale_px(400.0f);
    ComPtr<ID2D1RadialGradientBrush> glow;
    D2D1_GRADIENT_STOP glow_stops[2] = {
        {0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.06f)},
        {1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f)},
    };
    ComPtr<ID2D1GradientStopCollection> glow_coll;
    ctx->CreateGradientStopCollection(glow_stops, 2, &glow_coll);
    ctx->CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(cx, cy),
            D2D1::Point2F(),
            radius, radius),
        glow_coll.Get(), &glow);
    ctx->FillRectangle(m_rect, glow.Get());
}

void new_tab_dashboard::render_logo(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float logox = (m_rect.left + m_rect.right) / 2.0f;
    float logoy = m_rect.top + dpi.scale_px(80.0f);

    ComPtr<ID2D1SolidColorBrush> logob;
    ctx->CreateSolidColorBrush(colors.logo_gradient_start, &logob);

    // Draw a stylized infinity/nebula symbol as Hyperion logo
    float hw = dpi.scale_px(48.0f);
    D2D1_RECT_F logo_rect = D2D1::RectF(logox - hw, logoy - hw, logox + hw, logoy + hw);

    // Outer ring
    D2D1_ELLIPSE outer = D2D1::Ellipse(D2D1::Point2F(logox, logoy), hw, hw);
    ctx->DrawEllipse(outer, logob.Get(), dpi.scale_px(2.5f));

    // Inner filled circle
    D2D1_ELLIPSE inner = D2D1::Ellipse(D2D1::Point2F(logox, logoy), dpi.scale_px(8.0f), dpi.scale_px(8.0f));
    ctx->FillEllipse(inner, logob.Get());

    // "Hyperion" text below
    ComPtr<IDWriteTextFormat> hfmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &hfmt);
    hfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    D2D1_RECT_F label_r = D2D1::RectF(logox - dpi.scale_px(150.0f), logoy + hw + dpi.scale_px(8.0f),
                                        logox + dpi.scale_px(150.0f), logoy + hw + dpi.scale_px(32.0f));
    ctx->DrawText(L"Hyperion", 8, hfmt.Get(), label_r, logob.Get());
}

void new_tab_dashboard::render_search_bar(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float cx = (m_rect.left + m_rect.right) / 2.0f;
    float bar_w = dpi.scale_px(640.0f);
    float bar_h = dpi.scale_px(48.0f);
    float bar_x = cx - bar_w / 2.0f;
    float bar_y = m_rect.top + dpi.scale_px(170.0f);
    m_search_rect = D2D1::RectF(bar_x, bar_y, bar_x + bar_w, bar_y + bar_h);
    float radius = bar_h / 2.0f;

    // Outer glow (edge glow)
    float glow_w = dpi.scale_px(3.0f);
    ComPtr<ID2D1SolidColorBrush> glowb;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.5f, 1.0f, 0.35f), &glowb);
    ctx->DrawRoundedRectangle(D2D1::RoundedRect(m_search_rect, radius, radius), glowb.Get(), glow_w);

    // Inner background (glass)
    ComPtr<ID2D1SolidColorBrush> bg;
    ctx->CreateSolidColorBrush(colors.omnibox_bg, &bg);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(m_search_rect, radius, radius), bg.Get());

    // Border
    ComPtr<ID2D1SolidColorBrush> border;
    ctx->CreateSolidColorBrush(colors.omnibox_border, &border);
    ctx->DrawRoundedRectangle(D2D1::RoundedRect(m_search_rect, radius, radius), border.Get(), 1.0f);

    // Search icon
    float icx = bar_x + dpi.scale_px(16.0f);
    float icy = bar_y + bar_h / 2.0f;
    float is = dpi.scale_px(8.0f);
    ComPtr<ID2D1SolidColorBrush> ibr;
    ctx->CreateSolidColorBrush(colors.omnibox_icon, &ibr);
    D2D1_ELLIPSE mag = D2D1::Ellipse(D2D1::Point2F(icx, icy), is, is);
    ctx->DrawEllipse(mag, ibr.Get(), 1.5f);
    ctx->DrawLine(D2D1::Point2F(icx + is + 2, icy + is + 2),
                  D2D1::Point2F(icx + is + 6, icy + is + 6), ibr.Get(), 1.5f);

    // Placeholder text
    ComPtr<IDWriteTextFormat> pf;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &pf);
    pf->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    pf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    D2D1_RECT_F pl = D2D1::RectF(bar_x + dpi.scale_px(40.0f), bar_y,
                                  bar_x + bar_w - dpi.scale_px(16.0f), bar_y + bar_h);
    ComPtr<ID2D1SolidColorBrush> pbr;
    ctx->CreateSolidColorBrush(colors.omnibox_text_dimmed, &pbr);
    ctx->DrawText(L"Search or enter address", 24, pf.Get(), pl, pbr.Get());
}

void new_tab_dashboard::render_speed_dial(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float cx = (m_rect.left + m_rect.right) / 2.0f;
    float card_w = dpi.scale_px(160.0f);
    float card_h = dpi.scale_px(136.0f);
    float gap = dpi.scale_px(16.0f);
    int cols = 3;

    float total_w = cols * card_w + (cols - 1) * gap;
    float start_x = cx - total_w / 2.0f;
    float start_y = m_rect.top + dpi.scale_px(260.0f);
    float radius = dpi.scale_px(16.0f);

    for (size_t i = 0; i < m_speed_dials.size() && i < 6; i++) {
        int col = (int)i % cols;
        int row = (int)i / cols;
        float x = start_x + col * (card_w + gap);
        float y = start_y + row * (card_h + gap);
        D2D1_RECT_F card = D2D1::RectF(x, y, x + card_w, y + card_h);

        bool hovered = ((int)i == m_hovered_card);

        // Shadow
        D2D1_RECT_F shad = D2D1::RectF(card.left + 1, card.top + 3, card.right + 1, card.bottom + 3);
        ComPtr<ID2D1SolidColorBrush> shb;
        ctx->CreateSolidColorBrush(colors.card_shadow, &shb);
        ctx->FillRoundedRectangle(D2D1::RoundedRect(shad, radius, radius), shb.Get());

        // Card background
        ComPtr<ID2D1SolidColorBrush> cb;
        ctx->CreateSolidColorBrush(colors.card_bg, &cb);
        ctx->FillRoundedRectangle(D2D1::RoundedRect(card, radius, radius), cb.Get());

        // Border
        ComPtr<ID2D1SolidColorBrush> bb;
        ctx->CreateSolidColorBrush(colors.divider, &bb);
        ctx->DrawRoundedRectangle(D2D1::RoundedRect(card, radius, radius), bb.Get(), 0.5f);

        // Hover highlight
        if (hovered) {
            ComPtr<ID2D1SolidColorBrush> hl;
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.5f, 1.0f, 0.1f), &hl);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(card, radius, radius), hl.Get());
        }

        // Favicon placeholder (first letter)
        float fsz = dpi.scale_px(36.0f);
        float ffx = x + card_w / 2.0f - fsz / 2.0f;
        float ffy = y + dpi.scale_px(20.0f);
        D2D1_RECT_F fav_r = D2D1::RectF(ffx, ffy, ffx + fsz, ffy + fsz);

        ComPtr<ID2D1SolidColorBrush> fbb;
        ctx->CreateSolidColorBrush(colors.card_text, &fbb);
        ComPtr<ID2D1SolidColorBrush> fbbg;
        ctx->CreateSolidColorBrush(colors.card_bg, &fbbg);
        ctx->FillRoundedRectangle(D2D1::RoundedRect(fav_r, dpi.scale_px(10.0f), dpi.scale_px(10.0f)), fbbg.Get());

        ComPtr<IDWriteTextFormat> ffmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Inter",
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(18.0f), L"en-us", &ffmt);
        ffmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        ffmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        wchar_t letter[2] = { L'?', 0 };
        if (!m_speed_dials[i].title.empty()) {
            letter[0] = m_speed_dials[i].title[0];
        }
        ctx->DrawText(letter, 1, ffmt.Get(), fav_r, fbb.Get());

        // Title
        D2D1_RECT_F tr = D2D1::RectF(x + dpi.scale_px(8.0f), y + dpi.scale_px(66.0f),
                                       x + card_w - dpi.scale_px(8.0f), y + dpi.scale_px(90.0f));
        ComPtr<IDWriteTextFormat> tfmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Inter",
            DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(12.0f), L"en-us", &tfmt);
        tfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        tfmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        ctx->DrawText(m_speed_dials[i].title.c_str(), (UINT32)m_speed_dials[i].title.length(),
                       tfmt.Get(), tr, fbb.Get());

        // URL
        D2D1_RECT_F ur = D2D1::RectF(x + dpi.scale_px(8.0f), y + dpi.scale_px(90.0f),
                                       x + card_w - dpi.scale_px(8.0f), y + card_h - dpi.scale_px(8.0f));
        ComPtr<IDWriteTextFormat> ufmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Inter",
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(10.0f), L"en-us", &ufmt);
        ufmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        ComPtr<ID2D1SolidColorBrush> usb;
        ctx->CreateSolidColorBrush(colors.card_subtext, &usb);
        ctx->DrawText(m_speed_dials[i].url.c_str(), (UINT32)m_speed_dials[i].url.length(),
                       ufmt.Get(), ur, usb.Get());
    }
}

void new_tab_dashboard::handle_resize(int width, int height) {
    m_rect = D2D1::RectF(0.0f, 0.0f, (float)width, (float)height);
}

void new_tab_dashboard::handle_mouse_down(int x, int y) {
    if (!m_visible) return;
    auto& dpi = dpi_manager::instance();

    float cx = (m_rect.left + m_rect.right) / 2.0f;
    float card_w = dpi.scale_px(160.0f);
    float card_h = dpi.scale_px(136.0f);
    float gap = dpi.scale_px(16.0f);
    int cols = 3;
    float total_w = cols * card_w + (cols - 1) * gap;
    float start_x = cx - total_w / 2.0f;
    float start_y = m_rect.top + dpi.scale_px(260.0f);

    for (size_t i = 0; i < m_speed_dials.size() && i < 6; i++) {
        int col = (int)i % cols;
        int row = (int)i / cols;
        float fx = start_x + col * (card_w + gap);
        float fy = start_y + row * (card_h + gap);
        D2D1_RECT_F card = D2D1::RectF(fx, fy, fx + card_w, fy + card_h);

        if (x >= card.left && x <= card.right && y >= card.top && y <= card.bottom) {
            if (m_navigate_cb) {
                m_navigate_cb(m_speed_dials[i].url);
            }
            return;
        }
    }
}

void new_tab_dashboard::handle_mouse_move(int x, int y) {
    if (!m_visible) return;
    auto& dpi = dpi_manager::instance();

    float cx = (m_rect.left + m_rect.right) / 2.0f;
    float card_w = dpi.scale_px(160.0f);
    float card_h = dpi.scale_px(136.0f);
    float gap = dpi.scale_px(16.0f);
    int cols = 3;
    float total_w = cols * card_w + (cols - 1) * gap;
    float start_x = cx - total_w / 2.0f;
    float start_y = m_rect.top + dpi.scale_px(260.0f);

    int prev = m_hovered_card;
    m_hovered_card = -1;

    for (size_t i = 0; i < m_speed_dials.size() && i < 6; i++) {
        int col = (int)i % cols;
        int row = (int)i / cols;
        float fx = start_x + col * (card_w + gap);
        float fy = start_y + row * (card_h + gap);
        D2D1_RECT_F card = D2D1::RectF(fx, fy, fx + card_w, fy + card_h);

        if (x >= card.left && x <= card.right && y >= card.top && y <= card.bottom) {
            m_hovered_card = (int)i;
            break;
        }
    }
}

bool new_tab_dashboard::hit_test(int x, int y) const {
    return m_visible && x >= m_rect.left && x <= m_rect.right
           && y >= m_rect.top && y <= m_rect.bottom;
}

} // namespace hyperion::ui
