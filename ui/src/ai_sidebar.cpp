#include <hyperion/ui/ai_sidebar.hpp>
#include <hyperion/ui/dwrite_helpers.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <hyperion/ui/dpi_manager.hpp>
#include <algorithm>
#include <codecvt>
#include <locale>

namespace hyperion::ui {

static std::string to_utf8(const std::wstring& ws) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws);
#pragma warning(pop)
}

static std::wstring to_wstring(const std::string& str) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
#pragma warning(pop)
}

ai_sidebar::ai_sidebar()
    : m_ai(std::make_unique<ai_client>()) {}

ai_sidebar::~ai_sidebar() = default;

void ai_sidebar::initialize_ai(const std::string& api_key, const std::string& model) {
    m_ai->initialize(api_key, model);
}

void ai_sidebar::toggle() {
    m_open = !m_open;
}

void ai_sidebar::open() {
    m_open = true;
}

void ai_sidebar::close() {
    m_open = false;
}

void ai_sidebar::add_message(bool from_user, const std::wstring& text) {
    m_messages.push_back({from_user, text});
    if (m_messages.size() > 50) m_messages.pop_front();
}

void ai_sidebar::send_to_ai() {
    if (m_input_text.empty() || m_waiting_for_ai) return;

    std::wstring user_text = m_input_text;
    m_input_text.clear();
    m_waiting_for_ai = true;

    add_message(true, user_text);

    // Add a loading placeholder
    add_message(false, L"...");

    std::string narrow_text = to_utf8(user_text);
    m_ai->send_message(narrow_text,
        [this](const std::string& response) {
            // Remove the loading placeholder and add real response
            if (!m_messages.empty() && m_messages.back().text == L"...") {
                m_messages.pop_back();
            }
            std::wstring wresp = to_wstring(response);
            add_message(false, wresp);
            m_waiting_for_ai = false;
        },
        [this](const std::string& error) {
            if (!m_messages.empty() && m_messages.back().text == L"...") {
                m_messages.pop_back();
            }
            std::wstring werr = to_wstring(error);
            add_message(false, L"Error: " + werr);
            m_waiting_for_ai = false;
        });
}

void ai_sidebar::render(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float eff_w = m_sidebar_w * m_anim_progress;

    if (m_anim_progress > 0.01f) {
        render_backdrop(r);

        D2D1_RECT_F clip_r = D2D1::RectF(
            (float)m_width - eff_w, 0, (float)m_width, (float)m_height);
        ctx->PushAxisAlignedClip(clip_r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        render_panel(r);
        render_header(r);
        render_messages(r);
        render_input(r);

        ctx->PopAxisAlignedClip();
    }

    render_toggle_button(r);
}

void ai_sidebar::render_backdrop(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;

    ComPtr<ID2D1SolidColorBrush> backdrop;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.3f * m_anim_progress), &backdrop);
    ctx->FillRectangle(D2D1::RectF(0, 0, (float)m_width, (float)m_height), backdrop.Get());
}

void ai_sidebar::render_panel(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float eff_w = m_sidebar_w * m_anim_progress;
    D2D1_RECT_F panel = D2D1::RectF(
        (float)m_width - eff_w, 0, (float)m_width, (float)m_height);

    ComPtr<ID2D1SolidColorBrush> bg;
    ctx->CreateSolidColorBrush(colors.sidebar_bg, &bg);
    ctx->FillRectangle(panel, bg.Get());

    ComPtr<ID2D1SolidColorBrush> border;
    ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.06f), &border);
    ctx->DrawLine(D2D1::Point2F(panel.left, panel.top),
                  D2D1::Point2F(panel.left, panel.bottom), border.Get(), 1.0f);
}

void ai_sidebar::render_header(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float eff_w = m_sidebar_w * m_anim_progress;
    float hdr_h = dpi.scale_px(48.0f);
    D2D1_RECT_F hdr = D2D1::RectF(
        (float)m_width - eff_w, 0, (float)m_width, hdr_h);

    ComPtr<ID2D1SolidColorBrush> hdr_bg;
    ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.03f), &hdr_bg);
    ctx->FillRectangle(hdr, hdr_bg.Get());

    ComPtr<IDWriteTextFormat> tf;
    create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(14.0f), L"en-us", &tf);
    tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RECT_F title_r = D2D1::RectF(hdr.left + dpi.scale_px(16.0f), hdr.top,
                                        hdr.right - dpi.scale_px(44.0f), hdr.bottom);
    ComPtr<ID2D1SolidColorBrush> tb;
    ctx->CreateSolidColorBrush(colors.sidebar_header_text, &tb);
    ctx->DrawText(L"AI Assistant", 12, tf.Get(), title_r, tb.Get());

    float cb_sz = dpi.scale_px(28.0f);
    D2D1_RECT_F close_b = D2D1::RectF(hdr.right - cb_sz - dpi.scale_px(8.0f),
                                        hdr.top + (hdr_h - cb_sz) / 2.0f,
                                        hdr.right - dpi.scale_px(8.0f),
                                        hdr.top + (hdr_h - cb_sz) / 2.0f + cb_sz);

    ComPtr<ID2D1SolidColorBrush> cb_br;
    ctx->CreateSolidColorBrush(colors.sidebar_header_text, &cb_br);

    float cx = close_b.left + cb_sz / 2.0f;
    float cy = close_b.top + cb_sz / 2.0f;
    float cs = dpi.scale_px(7.0f);
    ctx->DrawLine(D2D1::Point2F(cx - cs, cy - cs), D2D1::Point2F(cx + cs, cy + cs), cb_br.Get(), 1.5f);
    ctx->DrawLine(D2D1::Point2F(cx + cs, cy - cs), D2D1::Point2F(cx - cs, cy + cs), cb_br.Get(), 1.5f);
}

void ai_sidebar::render_messages(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float eff_w = m_sidebar_w * m_anim_progress;
    float hdr_h = dpi.scale_px(48.0f);
    float inp_h = m_input_height > 0 ? m_input_height : dpi.scale_px(44.0f);
    float msg_y = hdr_h + dpi.scale_px(8.0f);
    float msg_w = eff_w - dpi.scale_px(24.0f);
    float max_h = (float)m_height - hdr_h - inp_h - dpi.scale_px(24.0f);
    float msg_x = (float)m_width - eff_w + dpi.scale_px(12.0f);

    // Clip messages area
    ctx->PushAxisAlignedClip(D2D1::RectF(msg_x, msg_y, msg_x + msg_w, msg_y + max_h),
                              D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // Calculate total content height
    float total_h = 0;
    std::vector<float> heights;
    for (size_t i = 0; i < m_messages.size(); i++) {
        float txt_h = dpi.scale_px(36.0f) + ((float)m_messages[i].text.length() / 40.0f) * dpi.scale_px(18.0f);
        float msg_h = std::max(dpi.scale_px(36.0f), txt_h);
        heights.push_back(msg_h);
        total_h += msg_h + dpi.scale_px(8.0f);
    }

    // Scroll to bottom if content exceeds visible area
    float scroll_y = 0;
    if (total_h > max_h) {
        scroll_y = total_h - max_h;
    }

    float draw_y = msg_y - scroll_y;
    for (size_t i = 0; i < m_messages.size(); i++) {
        float msg_h = heights[i];
        D2D1_RECT_F bubble = D2D1::RectF(msg_x, draw_y, msg_x + msg_w, draw_y + msg_h);
        float radius = dpi.scale_px(10.0f);

        ComPtr<ID2D1SolidColorBrush> bb;
        if (m_messages[i].from_user) {
            ctx->CreateSolidColorBrush(colors.sidebar_user_bubble, &bb);
        } else {
            ctx->CreateSolidColorBrush(colors.sidebar_ai_bubble, &bb);
        }
        ctx->FillRoundedRectangle(D2D1::RoundedRect(bubble, radius, radius), bb.Get());

        D2D1_RECT_F tr = D2D1::RectF(msg_x + dpi.scale_px(10.0f), draw_y,
                                       msg_x + msg_w - dpi.scale_px(10.0f), draw_y + msg_h);
        ComPtr<IDWriteTextFormat> tf;
        create_text_format(r.write_factory(), L"Inter",
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(12.0f), L"en-us", &tf);
        ComPtr<ID2D1SolidColorBrush> tb;
        ctx->CreateSolidColorBrush(m_messages[i].from_user ? colors.sidebar_user_text : colors.sidebar_ai_text, &tb);
        ctx->DrawText(m_messages[i].text.c_str(), (UINT32)m_messages[i].text.length(),
                       tf.Get(), tr, tb.Get());

        draw_y += msg_h + dpi.scale_px(8.0f);
    }

    ctx->PopAxisAlignedClip();
}

void ai_sidebar::render_input(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float eff_w = m_sidebar_w * m_anim_progress;
    float inp_h = dpi.scale_px(44.0f);
    float inp_y = (float)m_height - inp_h - dpi.scale_px(8.0f);
    m_input_height = inp_h;

    D2D1_RECT_F inp = D2D1::RectF(
        (float)m_width - eff_w + dpi.scale_px(12.0f),
        inp_y,
        (float)m_width - dpi.scale_px(12.0f),
        inp_y + inp_h);
    float radius = inp_h / 2.0f;

    ComPtr<ID2D1SolidColorBrush> bg;
    ctx->CreateSolidColorBrush(colors.sidebar_input_bg, &bg);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(inp, radius, radius), bg.Get());

    ComPtr<ID2D1SolidColorBrush> border;
    ctx->CreateSolidColorBrush(colors.omnibox_border, &border);
    ctx->DrawRoundedRectangle(D2D1::RoundedRect(inp, radius, radius), border.Get(), 1.0f);

    // Typed text or placeholder
    D2D1_RECT_F tr = D2D1::RectF(inp.left + dpi.scale_px(14.0f), inp.top,
                                   inp.right - dpi.scale_px(14.0f), inp.bottom);
    ComPtr<IDWriteTextFormat> pf;
    create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(13.0f), L"en-us", &pf);
    pf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    if (m_input_text.empty() && !m_waiting_for_ai) {
        ComPtr<ID2D1SolidColorBrush> pb;
        ctx->CreateSolidColorBrush(colors.sidebar_input_placeholder, &pb);
        ctx->DrawText(L"Ask AI anything\u2026", 19, pf.Get(), tr, pb.Get());
    } else if (m_waiting_for_ai) {
        ComPtr<ID2D1SolidColorBrush> pb;
        ctx->CreateSolidColorBrush(colors.sidebar_input_placeholder, &pb);
        ctx->DrawText(L"Waiting for response\u2026", 25, pf.Get(), tr, pb.Get());
    } else {
        ComPtr<ID2D1SolidColorBrush> tb;
        ctx->CreateSolidColorBrush(colors.sidebar_user_text, &tb);
        ctx->DrawText(m_input_text.c_str(), (UINT32)m_input_text.length(), pf.Get(), tr, tb.Get());
    }
}

void ai_sidebar::render_toggle_button(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    float bsz = dpi.scale_px(36.0f);
    float bx = (float)m_width - bsz - dpi.scale_px(4.0f);
    float by = (float)m_height / 2.0f - bsz / 2.0f;

    D2D1_RECT_F btn = D2D1::RectF(bx, by, bx + bsz, by + bsz);

    ComPtr<ID2D1SolidColorBrush> bgb;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.17f, 0.19f, 0.9f), &bgb);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(btn, bsz/2, bsz/2), bgb.Get());

    float cx = bx + bsz / 2.0f;
    float cy = by + bsz / 2.0f;
    float s = dpi.scale_px(4.0f);
    ComPtr<ID2D1SolidColorBrush> ib;
    ctx->CreateSolidColorBrush(colors.omnibox_icon, &ib);
    ctx->DrawLine(D2D1::Point2F(cx, cy - s - 2), D2D1::Point2F(cx, cy + s + 2), ib.Get(), 1.5f);
    ctx->DrawLine(D2D1::Point2F(cx - s - 2, cy), D2D1::Point2F(cx + s + 2, cy), ib.Get(), 1.5f);
    ctx->DrawLine(D2D1::Point2F(cx - s, cy - s), D2D1::Point2F(cx + s, cy + s), ib.Get(), 1.0f);
    ctx->DrawLine(D2D1::Point2F(cx + s, cy - s), D2D1::Point2F(cx - s, cy + s), ib.Get(), 1.0f);
}

void ai_sidebar::handle_char(wchar_t c) {
    if (!m_open) return;
    if (m_waiting_for_ai) return;

    if (c == L'\r' || c == L'\n') {
        send_to_ai();
    } else if (c == L'\b') {
        if (!m_input_text.empty()) {
            m_input_text.pop_back();
        }
    } else if (c >= 32) {
        m_input_text += c;
    }
}

void ai_sidebar::handle_key_down(UINT vk) {
    if (!m_open) return;
    if (vk == VK_RETURN) {
        send_to_ai();
    } else if (vk == VK_BACK) {
        if (!m_input_text.empty() && !m_waiting_for_ai) {
            m_input_text.pop_back();
        }
    } else if (vk == VK_ESCAPE) {
        close();
    }
}

void ai_sidebar::handle_resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void ai_sidebar::handle_mouse_down(int x, int y) {
    if (!m_open) return;
    auto& dpi = dpi_manager::instance();
    float hdr_h = dpi.scale_px(48.0f);

    // Close button hit test — same math as render_header()
    float cb_sz = dpi.scale_px(28.0f);
    D2D1_RECT_F hdr = D2D1::RectF(
        (float)m_width - m_sidebar_w * m_anim_progress, 0,
        (float)m_width, hdr_h);
    D2D1_RECT_F close_b = D2D1::RectF(
        hdr.right - cb_sz - dpi.scale_px(8.0f),
        hdr.top + (hdr_h - cb_sz) / 2.0f,
        hdr.right - dpi.scale_px(8.0f),
        hdr.top + (hdr_h - cb_sz) / 2.0f + cb_sz);
    if (x >= close_b.left && x <= close_b.right &&
        y >= close_b.top && y <= close_b.bottom) {
        close();
    }

    // Focus input on click in input area
    float inp_y = (float)m_height - m_input_height - dpi.scale_px(8.0f);
    if (x >= (float)m_width - m_sidebar_w * m_anim_progress + dpi.scale_px(12.0f) &&
        x <= (float)m_width - dpi.scale_px(12.0f) &&
        y >= inp_y && y <= inp_y + m_input_height) {
        // Input area clicked
    }
}

void ai_sidebar::handle_mouse_move(int x, int y) {
    (void)x; (void)y;
}

} // namespace hyperion::ui
