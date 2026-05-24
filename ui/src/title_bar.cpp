#include <hyperion/ui/title_bar.hpp>
#include <hyperion/ui/dwrite_helpers.hpp>
#include <cmath>

namespace hyperion::ui {

title_bar::title_bar() = default;

void title_bar::render(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& dpi = dpi_manager::instance();
    auto& colors = theme_manager::instance().colors();

    float tb_h = dpi.scale_px((float)height());

    D2D1_RECT_F tb = D2D1::RectF(0, 0, (float)m_width, tb_h);
    ComPtr<ID2D1SolidColorBrush> bg;
    ctx->CreateSolidColorBrush(colors.title_bar_bg, &bg);
    ctx->FillRectangle(tb, bg.Get());

    render_traffic_lights(r);
    render_logo(r);
}

void title_bar::render_traffic_lights(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& dpi = dpi_manager::instance();

    float x = dpi.scale_px(BTN_X_START);
    float y = dpi.scale_px(BTN_Y);
    float s = dpi.scale_px(BTN_SIZE);
    float g = dpi.scale_px(BTN_GAP);

    struct TL { float r, g, b; float hr, hg, hb; };
    TL colors[3] = {
        {1.0f, 0.23f, 0.23f, 0.9f, 0.15f, 0.15f},
        {1.0f, 0.80f, 0.00f, 0.9f, 0.70f, 0.0f},
        {0.18f, 0.80f, 0.18f, 0.10f, 0.70f, 0.10f}
    };

    for (int i = 0; i < 3; i++) {
        D2D1_POINT_2F c = D2D1::Point2F(x + i * (s + g) + s / 2, y + s / 2);
        float r2 = s / 2;

        if (i == m_hovered_btn) {
            ComPtr<ID2D1SolidColorBrush> glow;
            ctx->CreateSolidColorBrush(D2D1::ColorF(colors[i].hr, colors[i].hg, colors[i].hb, 0.3f), &glow);
            ctx->FillEllipse(D2D1::Ellipse(c, r2 + 4.0f, r2 + 4.0f), glow.Get());
        }

        ComPtr<ID2D1SolidColorBrush> brush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(colors[i].r, colors[i].g, colors[i].b, 0.9f), &brush);
        ctx->FillEllipse(D2D1::Ellipse(c, r2, r2), brush.Get());

        ComPtr<ID2D1SolidColorBrush> hl;
        ctx->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.3f), &hl);
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(c.x - r2 * 0.2f, c.y - r2 * 0.2f), r2 * 0.5f, r2 * 0.3f), hl.Get());
    }
}

void title_bar::render_logo(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& dpi = dpi_manager::instance();
    auto& colors = theme_manager::instance().colors();

    float logo_y = dpi.scale_px(9.0f);
    float logo_size = dpi.scale_px(18.0f);
    float tx = dpi.scale_px(90.0f);
    float cx = tx + logo_size / 2;
    float cy = logo_y + logo_size / 2;

    render_hyperion_ring(r, cx, cy, logo_size / 2 - 1);

    ComPtr<IDWriteTextFormat> fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_SEMI_LIGHT, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &fmt);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RECT_F tr = D2D1::RectF(tx + logo_size + dpi.scale_px(8.0f), dpi.scale_px(6.0f),
                                   (float)m_width, dpi.scale_px(36.0f));
    ComPtr<ID2D1SolidColorBrush> tb;
    ctx->CreateSolidColorBrush(colors.text_primary, &tb);
    ctx->DrawText(L"Hyperion", 8, fmt.Get(), tr, tb.Get());
}

void title_bar::render_hyperion_ring(renderer& r, float cx, float cy, float radius) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();

    ComPtr<ID2D1SolidColorBrush> ring;
    ctx->CreateSolidColorBrush(colors.accent, &ring);
    ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), ring.Get(), 1.8f);

    ComPtr<ID2D1SolidColorBrush> dot;
    ctx->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.8f), &dot);
    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + radius * 0.7f, cy - radius * 0.5f), 2.0f, 2.0f), dot.Get());

    ComPtr<ID2D1SolidColorBrush> arc;
    ctx->CreateSolidColorBrush(colors.accent_variant, &arc);
    ctx->DrawArc(D2D1::Point2F(cx - radius * 0.3f, cy + radius * 0.7f),
                 3.0f, 1.5f, 0.0f, 180.0f, arc.Get());
}

void title_bar::handle_mouse_move(int x, int y) {
    m_hovered_btn = -1;
    auto& dpi = dpi_manager::instance();
    float bx = dpi.scale_px(BTN_X_START);
    float by = dpi.scale_px(BTN_Y);
    float s = dpi.scale_px(BTN_SIZE);
    float g = dpi.scale_px(BTN_GAP);
    for (int i = 0; i < 3; i++) {
        float cx = bx + i * (s + g) + s / 2;
        float cy = by + s / 2;
        float dx = (float)x - cx;
        float dy = (float)y - cy;
        if (dx * dx + dy * dy <= (s / 2) * (s / 2)) {
            m_hovered_btn = i;
            break;
        }
    }
}

void title_bar::handle_mouse_down(int x, int y) {
    auto& dpi = dpi_manager::instance();
    float bx = dpi.scale_px(BTN_X_START);
    float by = dpi.scale_px(BTN_Y);
    float s = dpi.scale_px(BTN_SIZE);
    float g = dpi.scale_px(BTN_GAP);
    for (int i = 0; i < 3; i++) {
        float cx = bx + i * (s + g) + s / 2;
        float cy = by + s / 2;
        float dx = (float)x - cx;
        float dy = (float)y - cy;
        if (dx * dx + dy * dy <= (s / 2) * (s / 2)) {
            if (i == 0 && m_close_cb) m_close_cb();
            else if (i == 1 && m_minimize_cb) m_minimize_cb();
            else if (i == 2 && m_maximize_cb) m_maximize_cb();
            break;
        }
    }
}

bool title_bar::hit_close(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float bx = dpi.scale_px(BTN_X_START);
    float by = dpi.scale_px(BTN_Y);
    float s = dpi.scale_px(BTN_SIZE) / 2;
    float dx = (float)x - (bx + s);
    float dy = (float)y - (by + s);
    return dx * dx + dy * dy <= s * s;
}

bool title_bar::hit_minimize(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float bx = dpi.scale_px(BTN_X_START);
    float by = dpi.scale_px(BTN_Y);
    float s = dpi.scale_px(BTN_SIZE) / 2;
    float g = dpi.scale_px(BTN_GAP);
    float dx = (float)x - (bx + s + g + s);
    float dy = (float)y - (by + s);
    return dx * dx + dy * dy <= s * s;
}

bool title_bar::hit_maximize(int x, int y) const {
    auto& dpi = dpi_manager::instance();
    float bx = dpi.scale_px(BTN_X_START);
    float by = dpi.scale_px(BTN_Y);
    float s = dpi.scale_px(BTN_SIZE) / 2;
    float g = dpi.scale_px(BTN_GAP);
    float dx = (float)x - (bx + s + 2 * (s + g) + s);
    float dy = (float)y - (by + s);
    return dx * dx + dy * dy <= s * s;
}

} // namespace hyperion::ui
