#include <hyperion/ui/weather_widget.hpp>
#include <hyperion/ui/dwrite_helpers.hpp>
#include <hyperion/platform/logging.hpp>
#include <winhttp.h>
#include <sstream>
#include <thread>

#pragma comment(lib, "winhttp.lib")

namespace hyperion::ui {

static std::string narrow(const std::wstring& ws) {
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), len, nullptr, nullptr);
    return s;
}

static std::wstring widen(const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring ws(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), len);
    return ws;
}

// Simple JSON string parser - extracts value by key
static std::string json_extract_string(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + key.size() + 2);
    if (pos == std::string::npos) return {};
    pos = json.find_first_of("\"0123456789-", pos);
    if (pos == std::string::npos) return {};
    if (json[pos] == '"') {
        pos++;
        auto end = json.find('"', pos);
        if (end == std::string::npos) return {};
        return json.substr(pos, end - pos);
    }
    auto end = json.find_first_of(",}]", pos);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos);
}

static double json_extract_double(const std::string& json, const std::string& key) {
    auto s = json_extract_string(json, key);
    return s.empty() ? 0.0 : std::stod(s);
}

weather_widget::weather_widget() = default;
weather_widget::~weather_widget() = default;

void weather_widget::initialize(const std::wstring& api_key) {
    m_api_key = api_key;
    m_location_acquired = false;
    refresh();
}

void weather_widget::refresh() {
    if (m_loading) return;
    m_loading = true;
    m_error = false;

    if (!m_location_acquired) {
        m_lat = 40.7128;
        m_lon = -74.0060;
        m_location_acquired = true;
    }

    std::thread([this]() {
        fetch_weather();
        m_loading = false;
    }).detach();
}

void weather_widget::fetch_weather() {
    std::string url = "/data/2.5/weather?lat=" + std::to_string(m_lat)
        + "&lon=" + std::to_string(m_lon)
        + "&appid=" + narrow(m_api_key)
        + "&units=metric";

    HINTERNET hSession = WinHttpOpen(L"Hyperion/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) { m_error = true; return; }

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.openweathermap.org",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); m_error = true; return; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
        widen(url).c_str(), nullptr, nullptr, nullptr,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_error = true;
        return;
    }

    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    std::string response;
    if (ok) {
        DWORD bytes = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytes) && bytes > 0) {
            std::vector<char> buf(bytes + 1, 0);
            DWORD read = 0;
            WinHttpReadData(hRequest, buf.data(), bytes, &read);
            response.append(buf.data(), read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!response.empty()) {
        parse_response(response);
    } else {
        m_error = true;
    }
}

void weather_widget::parse_response(const std::string& json) {
    auto name = json_extract_string(json, "name");
    auto temp_s = json_extract_string(json, "temp");
    auto cond = json_extract_string(json, "description");
    auto icon = json_extract_string(json, "icon");

    m_data.location = widen(name);
    m_data.temp_c = temp_s.empty() ? 0 : std::stod(temp_s);
    m_data.temp_f = m_data.temp_c * 9.0 / 5.0 + 32.0;
    m_data.condition = widen(cond);
    m_data.icon = widen(icon);
    m_data.valid = true;
    m_error = false;
}

void weather_widget::render(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;

    if (m_loading && !m_data.valid) {
        render_loading(r);
    } else {
        render_card(r);
    }
}

void weather_widget::render_card(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F card = m_rect;
    float radius = dpi.scale_px(14.0f);

    // Shadow
    D2D1_RECT_F shad = D2D1::RectF(card.left + 2, card.top + 3, card.right + 2, card.bottom + 3);
    ComPtr<ID2D1SolidColorBrush> shadow_brush;
    ctx->CreateSolidColorBrush(colors.card_shadow, &shadow_brush);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(shad, radius, radius), shadow_brush.Get());

    // Glassmorphism card
    ComPtr<ID2D1SolidColorBrush> bg_brush;
    ctx->CreateSolidColorBrush(colors.card_bg, &bg_brush);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(card, radius, radius), bg_brush.Get());

    ComPtr<ID2D1SolidColorBrush> border_brush;
    ctx->CreateSolidColorBrush(colors.divider, &border_brush);
    ctx->DrawRoundedRectangle(D2D1::RoundedRect(card, radius, radius), border_brush.Get(), 0.5f);

    // Weather icon (emoji fallback)
    float icon_sz = dpi.scale_px(32.0f);
    D2D1_RECT_F icon_r = D2D1::RectF(card.left + dpi.scale_px(12.0f), card.top + dpi.scale_px(12.0f),
                                       card.left + dpi.scale_px(12.0f) + icon_sz,
                                       card.top + dpi.scale_px(12.0f) + icon_sz);

    const wchar_t* emoji = L"\u2600\uFE0F";
    if (m_data.condition.find(L"cloud") != std::wstring::npos) emoji = L"\u2601\uFE0F";
    else if (m_data.condition.find(L"rain") != std::wstring::npos) emoji = L"\U0001F327\uFE0F";
    else if (m_data.condition.find(L"snow") != std::wstring::npos) emoji = L"\u2744\uFE0F";
    else if (m_data.condition.find(L"thunder") != std::wstring::npos) emoji = L"\u26C8\uFE0F";
    else if (m_data.condition.find(L"drizzle") != std::wstring::npos) emoji = L"\U0001F326\uFE0F";
    else if (m_data.condition.find(L"mist") != std::wstring::npos || m_data.condition.find(L"fog") != std::wstring::npos) emoji = L"\U0001F32B\uFE0F";

    ComPtr<IDWriteTextFormat> emoji_fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Segoe UI Emoji",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(28.0f), L"en-us", &emoji_fmt);
    ComPtr<ID2D1SolidColorBrush> icon_brush;
    ctx->CreateSolidColorBrush(colors.card_text, &icon_brush);
    ctx->DrawText(emoji, 2, emoji_fmt.Get(), icon_r, icon_brush.Get());

    // Temperature
    float temp_x = icon_r.right + dpi.scale_px(10.0f);
    D2D1_RECT_F temp_r = D2D1::RectF(temp_x, card.top + dpi.scale_px(8.0f),
                                       card.right - dpi.scale_px(12.0f),
                                       card.top + dpi.scale_px(34.0f));
    ComPtr<IDWriteTextFormat> temp_fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(22.0f), L"en-us", &temp_fmt);
    ComPtr<ID2D1SolidColorBrush> temp_brush;
    ctx->CreateSolidColorBrush(colors.card_text, &temp_brush);
    std::wstring temp_str = std::to_wstring((int)m_data.temp_c) + L"\u00B0C";
    ctx->DrawText(temp_str.c_str(), (UINT32)temp_str.length(), temp_fmt.Get(), temp_r, temp_brush.Get());

    // Location + condition
    D2D1_RECT_F loc_r = D2D1::RectF(temp_x, card.top + dpi.scale_px(34.0f),
                                      card.right - dpi.scale_px(12.0f),
                                      card.top + dpi.scale_px(52.0f));
    ComPtr<IDWriteTextFormat> loc_fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(11.0f), L"en-us", &loc_fmt);
    ComPtr<ID2D1SolidColorBrush> sub_brush;
    ctx->CreateSolidColorBrush(colors.card_subtext, &sub_brush);
    std::wstring loc_str = m_data.location + L" \u2022 " + m_data.condition;
    ctx->DrawText(loc_str.c_str(), (UINT32)loc_str.length(), loc_fmt.Get(), loc_r, sub_brush.Get());
}

void weather_widget::render_loading(renderer& r) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();
    auto& dpi = dpi_manager::instance();

    D2D1_RECT_F card = m_rect;
    float radius = dpi.scale_px(14.0f);

    ComPtr<ID2D1SolidColorBrush> bg_brush;
    ctx->CreateSolidColorBrush(colors.card_bg, &bg_brush);
    ctx->FillRoundedRectangle(D2D1::RoundedRect(card, radius, radius), bg_brush.Get());

    ComPtr<IDWriteTextFormat> fmt;
    hyperion::ui::create_text_format(r.write_factory(), L"Inter",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, dpi.scale_px(13.0f), L"en-us", &fmt);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    ComPtr<ID2D1SolidColorBrush> tb;
    ctx->CreateSolidColorBrush(colors.card_subtext, &tb);
    ctx->DrawText(L"Loading weather\u2026", 18, fmt.Get(), card, tb.Get());
}

void weather_widget::handle_resize(int width, int height) {
    (void)width; (void)height;
}

void weather_widget::handle_mouse_down(int x, int y) {
    (void)x; (void)y;
}

} // namespace hyperion::ui
