#include <hyperion/ui/modern_bookmarks_bar.hpp>
#include <hyperion/ui/dwrite_helpers.hpp>
#include <hyperion/platform/logging.hpp>

namespace hyperion::ui {

modern_bookmarks_bar::modern_bookmarks_bar() {
    add_bookmark(L"Google", L"https://www.google.com");
    add_bookmark(L"YouTube", L"https://www.youtube.com");
    add_bookmark(L"GitHub", L"https://www.github.com");
    add_bookmark(L"Reddit", L"https://www.reddit.com");
}

void modern_bookmarks_bar::add_bookmark(const std::wstring& title, const std::wstring& url) {
    for (const auto& b : m_bookmarks) {
        if (b.url == url) return;
    }
    m_bookmarks.push_back({title, url, L"", {}, 0.0f});
}

void modern_bookmarks_bar::remove_bookmark(const std::wstring& url) {
    m_bookmarks.erase(
        std::remove_if(m_bookmarks.begin(), m_bookmarks.end(),
            [&](const auto& b) { return b.url == url; }),
        m_bookmarks.end()
    );
}

bool modern_bookmarks_bar::has_bookmark(const std::wstring& url) const {
    for (const auto& b : m_bookmarks) {
        if (b.url == url) return true;
    }
    return false;
}

void modern_bookmarks_bar::render(renderer& r) {
    if (!m_visible) return;
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F bg = D2D1::RectF(0, 0, (float)m_width, (float)m_height);
    ComPtr<ID2D1SolidColorBrush> bg_brush;
    ctx->CreateSolidColorBrush(colors.background, &bg_brush);
    ctx->FillRectangle(bg, bg_brush.Get());

    ComPtr<ID2D1SolidColorBrush> div_brush;
    ctx->CreateSolidColorBrush(colors.divider, &div_brush);
    ctx->DrawLine(D2D1::Point2F(0, (float)m_height - 0.5f),
                  D2D1::Point2F((float)m_width, (float)m_height - 0.5f),
                  div_brush.Get(), 1.0f);

    float x = dpi.scale_px(8.0f);
    float y = dpi.scale_px(3.0f);
    float item_h = (float)m_height - dpi.scale_px(6.0f);

    for (int i = 0; i < (int)m_bookmarks.size(); i++) {
        auto& bm = m_bookmarks[i];
        float text_w = (float)bm.title.length() * dpi.scale_px(7.0f);
        float item_w = text_w + dpi.scale_px(32.0f);

        D2D1_RECT_F card_r = D2D1::RectF((FLOAT)x, (FLOAT)y, (FLOAT)(x + item_w), (FLOAT)(y + item_h));
        bm.rect = card_r;

        bool hovered = (i == m_hovered_index);
        if (hovered) {
            ComPtr<ID2D1SolidColorBrush> hover_br;
            ctx->CreateSolidColorBrush(colors.hover_overlay, &hover_br);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(card_r, dpi.scale_px(6.0f), dpi.scale_px(6.0f)), hover_br.Get());
        }

        // Favicon circle
        D2D1_ELLIPSE fav_circle = D2D1::Ellipse(
            D2D1::Point2F(card_r.left + dpi.scale_px(12.0f), card_r.top + item_h / 2),
            dpi.scale_px(7.0f), dpi.scale_px(7.0f));
        ComPtr<ID2D1SolidColorBrush> fav_br;
        ctx->CreateSolidColorBrush(colors.accent, &fav_br);
        ctx->FillEllipse(fav_circle, fav_br.Get());

        // First letter of bookmark title
        ComPtr<ID2D1SolidColorBrush> letter_br;
        ctx->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &letter_br);
        ComPtr<IDWriteTextFormat> lfmt;
        hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Variable",
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(9.0f), L"en-us", &lfmt);
        lfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        lfmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        D2D1_RECT_F lr = D2D1::RectF(fav_circle.point.x - dpi.scale_px(7.0f), fav_circle.point.y - dpi.scale_px(7.0f),
                                       fav_circle.point.x + dpi.scale_px(7.0f), fav_circle.point.y + dpi.scale_px(7.0f));
        wchar_t first = bm.title.empty() ? L'?' : bm.title[0];
        ctx->DrawText(&first, 1, lfmt.Get(), lr, letter_br.Get());

        // Title text
        D2D1_RECT_F tr = D2D1::RectF(card_r.left + dpi.scale_px(24.0f), card_r.top,
                                       card_r.right - dpi.scale_px(4.0f), card_r.bottom);
        ComPtr<IDWriteTextFormat> tf;
        hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Variable",
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(11.5f), L"en-us", &tf);
        tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        tf->SetTrimming(&trim, nullptr);

        ComPtr<ID2D1SolidColorBrush> tb;
        ctx->CreateSolidColorBrush(colors.bookmark_text, &tb);
        ctx->PushAxisAlignedClip(card_r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        ctx->DrawText(bm.title.c_str(), (UINT32)bm.title.length(), tf.Get(), tr, tb.Get());
        ctx->PopAxisAlignedClip();

        x += item_w + dpi.scale_px(4.0f);
    }
}

void modern_bookmarks_bar::handle_resize(int width, int height) {
    m_width = width;
    (void)height;
}

void modern_bookmarks_bar::handle_mouse_down(int x, int y) {
    for (const auto& bm : m_bookmarks) {
        if (x >= bm.rect.left && x <= bm.rect.right &&
            y >= bm.rect.top && y <= bm.rect.bottom) {
            if (m_click_cb) m_click_cb(bm.url);
            return;
        }
    }
}

void modern_bookmarks_bar::handle_mouse_move(int x, int y) {
    m_hovered_index = -1;
    for (int i = 0; i < (int)m_bookmarks.size(); i++) {
        const auto& bm = m_bookmarks[i];
        if (x >= bm.rect.left && x <= bm.rect.right &&
            y >= bm.rect.top && y <= bm.rect.bottom) {
            m_hovered_index = i;
            return;
        }
    }
}

} // namespace hyperion::ui
