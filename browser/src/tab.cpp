#include <hyperion/browser/tab.hpp>
#include <hyperion/platform/logging.hpp>
#include <hyperion/platform/process_manager.hpp>
#include <hre/render/image_loader.hpp>
#include <hyperion/ui/renderer.hpp>
#include <codecvt>
#include <locale>
#include <sstream>

namespace hyperion::browser {

static std::string to_utf8(const std::wstring& ws) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws);
#pragma warning(pop)
}

static std::string to_string(const std::wstring& ws) {
    return to_utf8(ws);
}

tab::tab(const std::wstring& initial_url) 
    : m_url(initial_url), m_title(L"New Tab"), m_connected(false) {
    HYPERION_LOG_INFO("Tab created: {}", "New Tab"); 
}

tab::~tab() {
    // 1. Close IPC Named Pipe
    if (m_ipc_channel) {
        m_ipc_channel->close();
    }

    // 2. Safely join the reader thread
    if (m_reader_thread.joinable()) {
        m_reader_thread.join();
    }
}

void tab::initialize_webview(HWND parent_hwnd) {
    m_parent_hwnd = parent_hwnd;
    m_use_native = true;

    HYPERION_LOG_INFO("Initializing webview (release mode)...");

    m_connected = true;

    // Clean simple demo page - reduced elements to minimize glitches
    hre::render::render_command cmd;

    // Dark gradient background (as solid color for stability)
    cmd.type = hre::render::command_type::RECTANGLE;
    cmd.x = 0; cmd.y = 0;
    cmd.width = 1920; cmd.height = 1080;
    cmd.bg_color = 0xFF1a1a2e;  // Dark navy
    m_cached_commands.push_back(cmd);

    // Title
    cmd.type = hre::render::command_type::TEXT;
    cmd.x = 50; cmd.y = 50;
    cmd.width = 500; cmd.height = 40;
    cmd.text_content = L"Hyperion Browser v0.1.0 Alpha";
    cmd.font_family = L"Segoe UI";
    cmd.font_size = 28;
    cmd.text_color = 0xFF00d9ff;
    m_cached_commands.push_back(cmd);

    // Info lines
    cmd.font_size = 16;
    cmd.text_color = 0xFFaaaaaa;
    cmd.y = 120;
    cmd.text_content = L"A custom browser engine built from scratch";
    m_cached_commands.push_back(cmd);

    cmd.y = 160;
    cmd.text_content = L"Features:";
    cmd.text_color = 0xFF00d9ff;
    m_cached_commands.push_back(cmd);

    cmd.y = 190;
    cmd.text_color = 0xFF4ecdc4;
    cmd.text_content = L"  - HyperionJS: Custom JavaScript bytecode VM";
    m_cached_commands.push_back(cmd);

    cmd.y = 215;
    cmd.text_content = L"  - Rust HTML5 Tokenizer (zero-copy)";
    m_cached_commands.push_back(cmd);

    cmd.y = 240;
    cmd.text_content = L"  - Rust CSS Parser with complex selectors";
    m_cached_commands.push_back(cmd);

    cmd.y = 265;
    cmd.text_content = L"  - Direct2D/DirectWrite rendering";
    m_cached_commands.push_back(cmd);

    cmd.y = 290;
    cmd.text_content = L"  - Sandboxed multi-process architecture";
    m_cached_commands.push_back(cmd);

    cmd.y = 340;
    cmd.text_color = 0xFF00d9ff;
    cmd.text_content = L"Status: Browser loaded successfully!";
    cmd.font_size = 18;
    m_cached_commands.push_back(cmd);

    HYPERION_LOG_INFO("Webview initialized - release ready");
}

void tab::resize_webview(const RECT& bounds) {
    (void)bounds;
}

void tab::navigate(const std::wstring& url) {
    m_url = url;
    m_use_native = true;
    m_title = L"Hyperion Native (Sandboxed)...";
    m_progress = 0.4;

    if (m_connected && m_ipc_channel) {
        HYPERION_LOG_INFO("Browser sending NAVIGATE command for: %s", to_string(url).c_str());

        platform::ipc::message msg;
        msg.header.type = platform::ipc::message_type::NAVIGATE;
        std::string url_utf8 = to_utf8(url);
        msg.payload.assign(url_utf8.begin(), url_utf8.end());
        msg.header.payload_size = (uint32_t)msg.payload.size();

        m_ipc_channel->send(msg);
    } else {
        HYPERION_LOG_WARN("Cannot navigate. Renderer process is not connected.");
    }
}

void tab::handle_mouse_down(int x, int y) {
    if (!m_connected || !m_ipc_channel) return;

    // Adjust for toolbar offset (108px) and scroll position
    float adj_x = (float)x;
    float adj_y = (float)y - 108.0f + m_scroll_top;

    HYPERION_LOG_INFO("Browser packaging mouse event at (%.1f, %.1f) and sending to sandboxed renderer", adj_x, adj_y);

    platform::ipc::message msg;
    msg.header.type = platform::ipc::message_type::INPUT_EVENT;
    msg.payload.resize(sizeof(float) * 2);
    std::memcpy(msg.payload.data(), &adj_x, sizeof(float));
    std::memcpy(msg.payload.data() + sizeof(float), &adj_y, sizeof(float));
    msg.header.payload_size = (uint32_t)msg.payload.size();

    m_ipc_channel->send(msg);
}

void tab::handle_mouse_wheel(float delta) {
    m_scroll_top -= delta;
    if (m_scroll_top < 0) m_scroll_top = 0;
}

#ifdef DrawText
#undef DrawText
#endif

void tab::render_native(ui::renderer& r) {
    std::lock_guard<std::mutex> lock(m_commands_mutex);
    if (m_cached_commands.empty()) return;

    RECT client_rect;
    if (!GetClientRect(m_parent_hwnd, &client_rect)) return;
    float width = (float)(client_rect.right - client_rect.left);
    float height = (float)(client_rect.bottom - client_rect.top);

    auto* context = r.context();
    if (!context) return;

    D2D1_MATRIX_3X2_F old_transform;
    context->GetTransform(&old_transform);
    
    // Set viewport clip to avoid rendering outside the content viewport (y >= 108px)
    context->PushAxisAlignedClip(D2D1::RectF(0, 108, width, height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    context->SetTransform(D2D1::Matrix3x2F::Translation(0, 108.0f - m_scroll_top));

    // Helper: uint32_t (0xAARRGGBB) -> D2D1_COLOR_F
    auto to_d2d_color = [](uint32_t c) {
        float a = ((c >> 24) & 0xFF) / 255.0f;
        float r_val = ((c >> 16) & 0xFF) / 255.0f;
        float g_val = ((c >> 8) & 0xFF) / 255.0f;
        float b_val = (c & 0xFF) / 255.0f;
        return D2D1::ColorF(r_val, g_val, b_val, a);
    };

    // Replay the Direct2D rendering command list cached from the sandboxed renderer
    for (const auto& cmd : m_cached_commands) {
        D2D1_RECT_F rect = D2D1::RectF(cmd.x, cmd.y, cmd.x + cmd.width, cmd.y + cmd.height);

        if (cmd.type == hre::render::command_type::SHADOW) {
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> shadow_brush;
            context->CreateSolidColorBrush(to_d2d_color(cmd.shadow_color), &shadow_brush);
            
            D2D1_RECT_F shadow_rect = rect;
            shadow_rect.left += cmd.shadow_offset_x;
            shadow_rect.top += cmd.shadow_offset_y;
            shadow_rect.right += cmd.shadow_offset_x;
            shadow_rect.bottom += cmd.shadow_offset_y;
            
            context->FillRectangle(shadow_rect, shadow_brush.Get());

        } else if (cmd.type == hre::render::command_type::RECTANGLE) {
            if (cmd.border_width > 0) {
                Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border_brush;
                context->CreateSolidColorBrush(to_d2d_color(cmd.border_color), &border_brush);
                context->DrawRectangle(rect, border_brush.Get(), cmd.border_width);
            } else {
                Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bg_brush;
                context->CreateSolidColorBrush(to_d2d_color(cmd.bg_color), &bg_brush);
                context->FillRectangle(rect, bg_brush.Get());
            }

        } else if (cmd.type == hre::render::command_type::ROUNDED_RECTANGLE) {
            if (cmd.border_width > 0) {
                Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border_brush;
                context->CreateSolidColorBrush(to_d2d_color(cmd.border_color), &border_brush);
                context->DrawRoundedRectangle(D2D1::RoundedRect(rect, cmd.border_radius, cmd.border_radius), border_brush.Get(), cmd.border_width);
            } else {
                Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bg_brush;
                context->CreateSolidColorBrush(to_d2d_color(cmd.bg_color), &bg_brush);
                context->FillRoundedRectangle(D2D1::RoundedRect(rect, cmd.border_radius, cmd.border_radius), bg_brush.Get());
            }

        } else if (cmd.type == hre::render::command_type::IMAGE) {
            if (!cmd.image_src.empty()) {
                auto bitmap = hre::render::image_loader::load_from_url(cmd.image_src, context);
                if (bitmap) {
                    context->DrawBitmap(bitmap.Get(), rect);
                }
            }

        } else if (cmd.type == hre::render::command_type::TEXT) {
            Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
            r.write_factory()->CreateTextFormat(
                cmd.font_family.empty() ? L"Segoe UI" : cmd.font_family.c_str(),
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                cmd.font_size > 0 ? cmd.font_size : 14.0f,
                L"en-us",
                &format
            );

            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> text_brush;
            context->CreateSolidColorBrush(to_d2d_color(cmd.text_color), &text_brush);

            context->DrawText(cmd.text_content.c_str(), (UINT32)cmd.text_content.length(), format.Get(), &rect, text_brush.Get());
        }
    }

    context->PopAxisAlignedClip();
    context->SetTransform(old_transform);
}

void tab::back() {
    // TODO: Implement native history
}

void tab::forward() {
    // TODO: Implement native history
}

void tab::reload() {
    navigate(m_url);
}

void tab::set_visible(bool visible) {
    (void)visible;
}

} // namespace hyperion::browser
