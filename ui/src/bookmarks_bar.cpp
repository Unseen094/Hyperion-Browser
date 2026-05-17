#include <hyperion/ui/bookmarks_bar.hpp>

namespace hyperion::ui {

bookmarks_bar::bookmarks_bar() {
    // Add some default bookmarks for testing
    add_bookmark(L"Google", L"https://www.google.com");
    add_bookmark(L"YouTube", L"https://www.youtube.com");
    add_bookmark(L"GitHub", L"https://www.github.com");
}

void bookmarks_bar::render(renderer& r) {
    auto* context = r.context();
    if (!context) return;

    // Light Mode Background
    D2D1_RECT_F rect = D2D1::RectF(0, 0, (float)m_width, (float)m_height);
    ComPtr<ID2D1SolidColorBrush> brush;
    context->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.0f), &brush);
    context->FillRectangle(rect, brush.Get());

    float x = 12.0f;
    float y = 2.0f;
    float item_h = (float)m_height - 4.0f;

    ComPtr<IDWriteTextFormat> text_format;
    r.write_factory()->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.5f, L"en-us", &text_format);
    text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    ComPtr<ID2D1SolidColorBrush> text_brush;
    context->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.3f, 0.35f, 1.0f), &text_brush);

    for (auto& bm : m_bookmarks) {
        float item_w = (float)bm.title.length() * 7.5f + 24.0f;
        D2D1_RECT_F item_rect = D2D1::RectF(x, y, x + item_w, y + item_h);
        
        // Hover effect foundation
        context->DrawText(bm.title.c_str(), (UINT32)bm.title.length(), text_format.Get(), item_rect, text_brush.Get());
        
        bm.rect = { (int)item_rect.left, (int)item_rect.top, (int)item_rect.right, (int)item_rect.bottom };
        x += item_w + 8.0f;
    }
}

void bookmarks_bar::handle_resize(int width, int height) {
    (void)height;
    m_width = width;
}

void bookmarks_bar::handle_mouse_down(int x, int y) {
    for (const auto& bm : m_bookmarks) {
        if (x >= bm.rect.left && x <= bm.rect.right && y >= bm.rect.top && y <= bm.rect.bottom) {
            if (m_click_cb) m_click_cb(bm.url);
            return;
        }
    }
}

void bookmarks_bar::add_bookmark(const std::wstring& title, const std::wstring& url) {
    m_bookmarks.push_back({ title, url, {0,0,0,0} });
}

} // namespace hyperion::ui
