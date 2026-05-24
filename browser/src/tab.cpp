#include <hyperion/browser/tab.hpp>
#include <hyperion/platform/logging.hpp>
#include <hre/render/image_loader.hpp>
#include <hyperion/ui/renderer.hpp>
#include <hre/storage/session_storage.hpp>

#include <hre/dom/node.hpp>
#include <hre/parser/html_parser.hpp>
#include <hre/layout/layout_engine.hpp>
#include <hre/css/style_engine.hpp>
#include <hre/net/request.hpp>
#include <hre/script/script_engine.hpp>

#include <codecvt>
#include <locale>
#include <sstream>
#include <functional>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace hyperion::browser {

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

static uint32_t css_color_to_uint32(const std::wstring& color) {
    if (color.empty() || color == L"transparent") return 0x00000000;
    if (color == L"red") return 0xFFFF0000;
    if (color == L"blue") return 0xFF0000FF;
    if (color == L"green") return 0xFF008000;
    if (color == L"black") return 0xFF000000;
    if (color == L"white") return 0xFFFFFFFF;
    if (color == L"gray") return 0xFF808080;
    if (color == L"dark-gray") return 0xFF333340;
    if (color == L"silver") return 0xFFC0C0C0;
    if (color == L"yellow") return 0xFFFFFF00;

    if (color.length() == 4 && color[0] == L'#') {
        auto hex_digit = [](wchar_t c) -> uint32_t {
            return (c >= L'0' && c <= L'9') ? (c - L'0') :
                   (c >= L'a' && c <= L'f') ? (c - L'a' + 10) :
                   (c >= L'A' && c <= L'F') ? (c - L'A' + 10) : 0;
        };
        uint32_t r = hex_digit(color[1]) * 17;
        uint32_t g = hex_digit(color[2]) * 17;
        uint32_t b = hex_digit(color[3]) * 17;
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    if (color.length() == 7 && color[0] == L'#') {
        auto hex_pair = [](wchar_t hi, wchar_t lo) -> uint32_t {
            auto hex_digit = [](wchar_t c) -> uint32_t {
                return (c >= L'0' && c <= L'9') ? (c - L'0') :
                       (c >= L'a' && c <= L'f') ? (c - L'a' + 10) :
                       (c >= L'A' && c <= L'F') ? (c - L'A' + 10) : 0;
            };
            return hex_digit(hi) * 16 + hex_digit(lo);
        };
        uint32_t r = hex_pair(color[1], color[2]);
        uint32_t g = hex_pair(color[3], color[4]);
        uint32_t b = hex_pair(color[5], color[6]);
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    return 0xFF333333;
}

// Extract two gradient colors from a linear-gradient(...) string
static void parse_gradient_colors(const std::wstring& gradient, uint32_t& c1, uint32_t& c2) {
    c1 = 0xFF1a1a2e;
    c2 = 0xFF16213e;
    size_t paren = gradient.find(L'(');
    if (paren == std::wstring::npos) return;
    size_t start = gradient.find(L'#', paren);
    if (start == std::wstring::npos) return;
    size_t end = start + 1;
    while (end < gradient.size() && (gradient[end] == L'#' || (gradient[end] >= L'0' && gradient[end] <= L'9') ||
           (gradient[end] >= L'a' && gradient[end] <= L'f') || (gradient[end] >= L'A' && gradient[end] <= L'F'))) end++;
    if (end == start + 1) return;
    c1 = css_color_to_uint32(gradient.substr(start, end - start));
    size_t start2 = gradient.find(L'#', end);
    if (start2 == std::wstring::npos) return;
    size_t end2 = start2 + 1;
    while (end2 < gradient.size() && (gradient[end2] == L'#' || (gradient[end2] >= L'0' && gradient[end2] <= L'9') ||
           (gradient[end2] >= L'a' && gradient[end2] <= L'f') || (gradient[end2] >= L'A' && gradient[end2] <= L'F'))) end2++;
    if (end2 == start2 + 1) return;
    c2 = css_color_to_uint32(gradient.substr(start2, end2 - start2));
}

static void generate_commands_recursive(const hre::layout::LayoutNode& node, std::vector<hre::render::render_command>& out_commands) {
    if (node.dom_node->type() == hre::dom::node_type::element) {
        auto* el = static_cast<const hre::dom::element*>(node.dom_node);
        const auto& s = node.style;

        if (s.display == L"none" || s.visibility == L"hidden") return;

        if (s.box_shadow_blur > 0) {
            hre::render::render_command cmd;
            cmd.type = hre::render::command_type::SHADOW;
            cmd.x = static_cast<float>(node.content_box.x);
            cmd.y = static_cast<float>(node.content_box.y);
            cmd.width = static_cast<float>(node.content_box.width);
            cmd.height = static_cast<float>(node.content_box.height);
            cmd.shadow_offset_x = static_cast<float>(s.box_shadow_offset_x);
            cmd.shadow_offset_y = static_cast<float>(s.box_shadow_offset_y);
            cmd.shadow_color = css_color_to_uint32(s.box_shadow_color);
            cmd.shadow_blur = static_cast<float>(s.box_shadow_blur);
            out_commands.push_back(cmd);
        }

        if (s.background_color != L"transparent" || !s.background_gradient.empty()) {
            hre::render::render_command cmd;
            cmd.type = s.border_radius > 0 ? hre::render::command_type::ROUNDED_RECTANGLE : hre::render::command_type::RECTANGLE;
            cmd.x = static_cast<float>(node.content_box.x);
            cmd.y = static_cast<float>(node.content_box.y);
            cmd.width = static_cast<float>(node.content_box.width);
            cmd.height = static_cast<float>(node.content_box.height);
            cmd.bg_color = css_color_to_uint32(s.background_color);
            cmd.border_radius = static_cast<float>(s.border_radius);
            if (!s.background_gradient.empty()) {
                uint32_t g1 = 0, g2 = 0;
                parse_gradient_colors(s.background_gradient, g1, g2);
                cmd.bg_color = g1;
                cmd.bg_color2 = g2;
            }
            out_commands.push_back(cmd);
        }

        if (el->tag_name() == L"img") {
            std::wstring src = el->get_attribute(L"src");
            if (!src.empty()) {
                hre::render::render_command cmd;
                cmd.type = hre::render::command_type::IMAGE;
                cmd.x = static_cast<float>(node.content_box.x);
                cmd.y = static_cast<float>(node.content_box.y);
                cmd.width = static_cast<float>(node.content_box.width);
                cmd.height = static_cast<float>(node.content_box.height);
                cmd.image_src = src;
                out_commands.push_back(cmd);
            }
        }

        if (el->tag_name() == L"input") {
            std::wstring type = el->get_attribute(L"type");
            if (type.empty() || type == L"text" || type == L"password" || type == L"search" || type == L"email") {
                hre::render::render_command bg;
                bg.type = hre::render::command_type::ROUNDED_RECTANGLE;
                bg.x = static_cast<float>(node.content_box.x);
                bg.y = static_cast<float>(node.content_box.y);
                bg.width = static_cast<float>(node.content_box.width);
                bg.height = static_cast<float>(node.content_box.height);
                bg.bg_color = 0xFFFFFFFF;
                bg.border_radius = 4.0f;
                out_commands.push_back(bg);

                hre::render::render_command border;
                border.type = hre::render::command_type::ROUNDED_RECTANGLE;
                border.x = static_cast<float>(node.content_box.x);
                border.y = static_cast<float>(node.content_box.y);
                border.width = static_cast<float>(node.content_box.width);
                border.height = static_cast<float>(node.content_box.height);
                border.border_color = 0xFF888888;
                border.border_width = 1.0f;
                border.border_radius = 4.0f;
                out_commands.push_back(border);

                std::wstring val = el->get_attribute(L"value");
                if (!val.empty()) {
                    hre::render::render_command txt;
                    txt.type = hre::render::command_type::TEXT;
                    txt.x = static_cast<float>(node.content_box.x) + 6.0f;
                    txt.y = static_cast<float>(node.content_box.y) + 4.0f;
                    txt.width = static_cast<float>(node.content_box.width) - 12.0f;
                    txt.height = static_cast<float>(node.content_box.height) - 8.0f;
                    txt.text_content = type == L"password" ? std::wstring(val.size(), L'*') : val;
                    txt.font_family = L"Segoe UI";
                    txt.font_size = 14.0f;
                    txt.text_color = 0xFF000000;
                    out_commands.push_back(txt);
                }
            } else if (type == L"submit" || type == L"button") {
                hre::render::render_command bg;
                bg.type = hre::render::command_type::ROUNDED_RECTANGLE;
                bg.x = static_cast<float>(node.content_box.x);
                bg.y = static_cast<float>(node.content_box.y);
                bg.width = static_cast<float>(node.content_box.width);
                bg.height = static_cast<float>(node.content_box.height);
                bg.bg_color = 0xFF00d9ff;
                bg.border_radius = 4.0f;
                out_commands.push_back(bg);

                std::wstring label = el->get_attribute(L"value");
                if (label.empty()) label = type == L"submit" ? L"Submit" : L"Button";
                hre::render::render_command txt;
                txt.type = hre::render::command_type::TEXT;
                txt.x = static_cast<float>(node.content_box.x) + 8.0f;
                txt.y = static_cast<float>(node.content_box.y) + 6.0f;
                txt.width = static_cast<float>(node.content_box.width) - 16.0f;
                txt.height = static_cast<float>(node.content_box.height) - 12.0f;
                txt.text_content = label;
                txt.font_family = L"Segoe UI";
                txt.font_size = 14.0f;
                txt.text_color = 0xFF1a1a2e;
                out_commands.push_back(txt);
            } else if (type == L"checkbox") {
                hre::render::render_command bg;
                bg.type = hre::render::command_type::RECTANGLE;
                bg.x = static_cast<float>(node.content_box.x) + 2.0f;
                bg.y = static_cast<float>(node.content_box.y) + 2.0f;
                bg.width = 16.0f;
                bg.height = 16.0f;
                bg.bg_color = 0xFFFFFFFF;
                out_commands.push_back(bg);

                hre::render::render_command border;
                border.type = hre::render::command_type::RECTANGLE;
                border.x = static_cast<float>(node.content_box.x) + 2.0f;
                border.y = static_cast<float>(node.content_box.y) + 2.0f;
                border.width = 16.0f;
                border.height = 16.0f;
                border.border_color = 0xFF888888;
                border.border_width = 1.0f;
                out_commands.push_back(border);

                if (el->get_attribute(L"checked") == L"true" || el->get_attribute(L"checked") == L"checked") {
                    hre::render::render_command check;
                    check.type = hre::render::command_type::TEXT;
                    check.x = static_cast<float>(node.content_box.x) + 2.0f;
                    check.y = static_cast<float>(node.content_box.y);
                    check.width = 16.0f;
                    check.height = 16.0f;
                    check.text_content = L"\u2713";
                    check.font_family = L"Segoe UI";
                    check.font_size = 14.0f;
                    check.text_color = 0xFF1a1a2e;
                    out_commands.push_back(check);
                }
            }
        }

        if (el->tag_name() == L"button") {
            std::wstring btn_type = el->get_attribute(L"type");
            if (btn_type.empty()) btn_type = L"submit";
            hre::render::render_command bg;
            bg.type = hre::render::command_type::ROUNDED_RECTANGLE;
            bg.x = static_cast<float>(node.content_box.x);
            bg.y = static_cast<float>(node.content_box.y);
            bg.width = static_cast<float>(node.content_box.width);
            bg.height = static_cast<float>(node.content_box.height);
            bg.bg_color = 0xFF00d9ff;
            bg.border_radius = 4.0f;
            out_commands.push_back(bg);

            std::wstring label;
            for (const auto& child : node.children) {
                if (child->dom_node->type() == hre::dom::node_type::text) {
                    label += static_cast<const hre::dom::text_node*>(child->dom_node)->text();
                }
            }
            if (label.empty()) label = L"Button";
            hre::render::render_command txt;
            txt.type = hre::render::command_type::TEXT;
            txt.x = static_cast<float>(node.content_box.x) + 8.0f;
            txt.y = static_cast<float>(node.content_box.y) + 6.0f;
            txt.width = static_cast<float>(node.content_box.width) - 16.0f;
            txt.height = static_cast<float>(node.content_box.height) - 12.0f;
            txt.text_content = label;
            txt.font_family = L"Segoe UI";
            txt.font_size = 14.0f;
            txt.text_color = 0xFF1a1a2e;
            out_commands.push_back(txt);
        }

        if (el->tag_name() == L"textarea") {
            hre::render::render_command bg;
            bg.type = hre::render::command_type::ROUNDED_RECTANGLE;
            bg.x = static_cast<float>(node.content_box.x);
            bg.y = static_cast<float>(node.content_box.y);
            bg.width = static_cast<float>(node.content_box.width);
            bg.height = static_cast<float>(node.content_box.height);
            bg.bg_color = 0xFFFFFFFF;
            bg.border_radius = 4.0f;
            out_commands.push_back(bg);

            hre::render::render_command border;
            border.type = hre::render::command_type::ROUNDED_RECTANGLE;
            border.x = static_cast<float>(node.content_box.x);
            border.y = static_cast<float>(node.content_box.y);
            border.width = static_cast<float>(node.content_box.width);
            border.height = static_cast<float>(node.content_box.height);
            border.border_color = 0xFF888888;
            border.border_width = 1.0f;
            border.border_radius = 4.0f;
            out_commands.push_back(border);

            std::wstring val = el->get_attribute(L"value");
            if (!val.empty()) {
                hre::render::render_command txt;
                txt.type = hre::render::command_type::TEXT;
                txt.x = static_cast<float>(node.content_box.x) + 6.0f;
                txt.y = static_cast<float>(node.content_box.y) + 4.0f;
                txt.width = static_cast<float>(node.content_box.width) - 12.0f;
                txt.height = static_cast<float>(node.content_box.height) - 8.0f;
                txt.text_content = val;
                txt.font_family = L"Segoe UI";
                txt.font_size = 14.0f;
                txt.text_color = 0xFF000000;
                out_commands.push_back(txt);
            }
        }

        float bw = static_cast<float>(s.border_top);
        if (bw > 0 && s.border_color != L"transparent") {
            hre::render::render_command cmd;
            cmd.type = s.border_radius > 0 ? hre::render::command_type::ROUNDED_RECTANGLE : hre::render::command_type::RECTANGLE;
            cmd.x = static_cast<float>(node.content_box.x);
            cmd.y = static_cast<float>(node.content_box.y);
            cmd.width = static_cast<float>(node.content_box.width);
            cmd.height = static_cast<float>(node.content_box.height);
            cmd.bg_color = 0;
            cmd.border_color = css_color_to_uint32(s.border_color);
            cmd.border_width = bw;
            cmd.border_radius = static_cast<float>(s.border_radius);
            out_commands.push_back(cmd);
        }

    } else if (node.dom_node->type() == hre::dom::node_type::text) {
        auto* text_node = static_cast<const hre::dom::text_node*>(node.dom_node);
        const auto& s = node.style;

        hre::render::render_command cmd;
        cmd.type = hre::render::command_type::TEXT;
        cmd.x = static_cast<float>(node.content_box.x);
        cmd.y = static_cast<float>(node.content_box.y);
        cmd.width = static_cast<float>(node.content_box.width);
        cmd.height = static_cast<float>(node.content_box.height);
        cmd.text_content = text_node->text();
        cmd.font_family = s.font_family.empty() ? L"Segoe UI" : s.font_family;
        cmd.font_size = static_cast<float>(s.font_size.to_pixels());
        cmd.text_color = css_color_to_uint32(s.color);
        out_commands.push_back(cmd);
    }

    for (const auto& child_ptr : node.children) {
        generate_commands_recursive(*child_ptr, out_commands);
    }
}

tab::tab(const std::wstring& initial_url)
    : m_url(initial_url), m_title(L"New Tab") {
    m_history.push_back(initial_url);
    m_history_index = 0;
    HYPERION_LOG_INFO("Tab created: {}", "New Tab");
}

tab::~tab() = default;

void tab::initialize_webview(HWND parent_hwnd) {
    m_parent_hwnd = parent_hwnd;
    m_use_native = true;

    HYPERION_LOG_INFO("Webview initialized for in-process rendering");

    hre::render::render_command cmd;
    cmd.type = hre::render::command_type::RECTANGLE;
    cmd.x = 0; cmd.y = 0;
    cmd.width = 1920; cmd.height = 1080;
    cmd.bg_color = 0xFF1a1a2e;
    {
        std::lock_guard<std::mutex> lock(m_commands_mutex);
        m_cached_commands.clear();
        m_cached_commands.push_back(cmd);
    }
}

void tab::resize_webview(const RECT& bounds) {
    m_bounds = bounds;
}

void tab::render_page_in_process(const std::wstring& url) {
    try {
        HYPERION_LOG_INFO("Rendering page in-process: %s", to_utf8(url).c_str());

        std::wstring html_content;
        if (url.find(L"native://home") == 0) {
            html_content =
                L"<html>"
                L"<head><title>Hyperion Browser</title></head>"
                L"<body>"
                L"  <div class='container'>"
                L"    <h1>Hyperion Browser</h1>"
                L"    <p class='subtitle'>A custom browser engine built from scratch</p>"
                L"    <div class='links'>"
                L"      <a href='native://demo'>Run Demo</a>"
                L"      <a href='native://test'>Test Page</a>"
                L"    </div>"
                L"    <div class='info'>"
                L"      <h2>Features</h2>"
                L"      <ul>"
                L"        <li>Custom JavaScript Engine (HyperionJS)</li>"
                L"        <li>Rust HTML/CSS Parsers</li>"
                L"        <li>Direct2D Rendering</li>"
                L"        <li>Secure Sandboxed Rendering</li>"
                L"      </ul>"
                L"    </div>"
                L"  </div>"
                L"</body>"
                L"</html>";
        } else if (url.find(L"native://demo") == 0) {
            html_content =
                L"<html>"
                L"<head><title>Hyperion Demo</title></head>"
                L"<body>"
                L"  <div class='container'>"
                L"    <h1>Demo Page</h1>"
                L"    <div class='card'>"
                L"      <h3>JavaScript Test</h3>"
                L"      <p id='js-test'>Testing...</p>"
                L"      <button onclick='document.getElementById(\"js-test\").innerText = \"JavaScript works!\"'>Click Me</button>"
                L"    </div>"
                L"    <div class='card'>"
                L"      <h3>Array Methods</h3>"
                L"      <p id='array-test'>Testing...</p>"
                L"    </div>"
                L"    <div class='card'>"
                L"      <h3>String Methods</h3>"
                L"      <p id='string-test'>Testing...</p>"
                L"    </div>"
                L"    <a href='native://home' class='back'>&larr; Back to Home</a>"
                L"  </div>"
                L"</body>"
                L"</html>";
        } else if (url.find(L"native://test") == 0) {
            html_content =
                L"<html>"
                L"<head><title>Hyperion Test</title></head>"
                L"<body>"
                L"  <div class='container'>"
                L"    <h1>HTML/CSS Test</h1>"
                L"    <div style='background: #ff6b6b; padding: 20px; margin: 10px 0; border-radius: 8px;'>Red Box</div>"
                L"    <div style='background: #4ecdc4; padding: 20px; margin: 10px 0; border-radius: 8px;'>Teal Box</div>"
                L"    <div style='background: #45b7d1; padding: 20px; margin: 10px 0; border-radius: 8px;'>Blue Box</div>"
                L"    <div class='flex-box'>"
                L"      <div>Item 1</div>"
                L"      <div>Item 2</div>"
                L"      <div>Item 3</div>"
                L"    </div>"
                L"    <a href='native://home' class='back'>&larr; Back to Home</a>"
                L"  </div>"
                L"</body>"
                L"</html>";
        } else if (url.find(L"native://settings") == 0) {
            html_content =
                L"<html>"
                L"<head><title>Settings - Hyperion Browser</title></head>"
                L"<body>"
                L"  <div class='container'>"
                L"    <h1>Settings</h1>"
                L"    <div class='card'>"
                L"      <h3>Appearance</h3>"
                L"      <p>Theme: System default (auto-detected)</p>"
                L"      <p>Window chrome: Custom acrylic titlebar</p>"
                L"    </div>"
                L"    <div class='card'>"
                L"      <h3>Search Engine</h3>"
                L"      <p>Default: Google</p>"
                L"    </div>"
                L"    <div class='card'>"
                L"      <h3>About</h3>"
                L"      <p>Hyperion Browser v0.2.0</p>"
                L"      <p>Custom browser engine built from scratch in C++23</p>"
                L"      <p>Rendering: Direct2D/DirectWrite with D3D11 swap chain</p>"
                L"    </div>"
                L"    <a href='native://home' class='back'>&larr; Back to Home</a>"
                L"  </div>"
                L"</body>"
                L"</html>";
        } else {
            html_content = hre::net::request::fetch(url);
        }

        if (html_content.empty()) {
            render_error_page(L"Empty response from server");
            m_progress = 0.0;
            return;
        }

        hre::parser::html_parser parser(to_utf8(html_content));
        auto document = parser.parse();
        if (!document) {
            render_error_page(L"Failed to parse HTML");
            m_progress = 0.0;
            return;
        }

        // Create script engine and execute inline scripts
        m_script_engine = std::make_unique<hre::script::script_engine>(document.get());
        m_script_engine->execute_scripts();

        // Extract title from document
        for (auto& child : document->children()) {
            if (child->type() == hre::dom::node_type::element) {
                auto* el = static_cast<hre::dom::element*>(child.get());
                if (el->tag_name() == L"head") {
                    for (auto& hc : el->children()) {
                        if (hc->type() == hre::dom::node_type::element) {
                            auto* he = static_cast<hre::dom::element*>(hc.get());
                            if (he->tag_name() == L"title" && !he->children().empty()) {
                                auto* tn = static_cast<hre::dom::text_node*>(he->children()[0].get());
                                m_title = tn->text();
                            }
                        }
                    }
                }
            }
        }

        std::wstring default_css =
            L"body { font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%); color: white; margin: 0; padding: 40px; }"
            L".container { max-width: 800px; margin: 0 auto; }"
            L"h1 { font-size: 48px; margin-bottom: 10px; color: #00d9ff; }"
            L".subtitle { font-size: 20px; color: #a0a0a0; margin-bottom: 40px; }"
            L"h2 { color: #00d9ff; margin-top: 30px; }"
            L"ul { color: #c0c0c0; line-height: 1.8; }"
            L"a { color: #00d9ff; text-decoration: none; margin: 10px 20px 10px 0; display: inline-block; padding: 12px 24px; background: #2d2d4a; border-radius: 8px; }"
            L"a:hover { background: #3d3d6a; }"
            L".links { margin: 30px 0; }"
            L".card { background: #2d2d4a; padding: 20px; margin: 20px 0; border-radius: 12px; }"
            L".card h3 { color: #00d9ff; margin-top: 0; }"
            L"button { background: #00d9ff; color: #1a1a2e; border: none; padding: 12px 24px; border-radius: 6px; cursor: pointer; font-size: 16px; }"
            L"button:hover { background: #00b8d9; }"
            L"p { color: #c0c0c0; }"
            L".flex-box { display: flex; gap: 20px; margin: 20px 0; }"
            L".flex-box div { background: #2d2d4a; padding: 30px; border-radius: 8px; flex: 1; text-align: center; }"
            L".back { background: #4a4a6a; }";

        float viewport_width = static_cast<float>(m_bounds.right - m_bounds.left);
        if (viewport_width < 1) viewport_width = 1000;

        m_layout_engine = std::make_unique<hre::layout::LayoutEngine>();
        m_layout_engine->load_raw_rules({ default_css });

        hre::css::ComputedStyle default_computed_style;
        default_computed_style.display = L"block";

        auto layout_root = m_layout_engine->build_layout_tree(document.get(), default_computed_style);
        if (!layout_root) {
            render_error_page(L"Failed to build layout tree");
            m_progress = 0.0;
            return;
        }

        m_layout_engine->layout_tree(layout_root, static_cast<double>(viewport_width), 0);

        m_layout_root = layout_root;
        m_y_offset = 0;
        build_click_targets(*m_layout_root, 0);

        std::vector<hre::render::render_command> commands;
        generate_commands_recursive(*layout_root, commands);

        // Set up CSS transition from old state to new state
        if (!m_cached_commands.empty() && !commands.empty()) {
            // Find first non-zero bg_color command in old and new sets
            uint32_t old_bg = 0xFF1a1a2e;
            for (const auto& oc : m_cached_commands) {
                if ((oc.type == hre::render::command_type::RECTANGLE ||
                     oc.type == hre::render::command_type::ROUNDED_RECTANGLE) &&
                    oc.bg_color != 0 && oc.bg_color != 0xFF1a1a2e) {
                    old_bg = oc.bg_color;
                    break;
                }
            }
            uint32_t new_bg = 0xFF1a1a2e;
            for (const auto& nc : commands) {
                if ((nc.type == hre::render::command_type::RECTANGLE ||
                     nc.type == hre::render::command_type::ROUNDED_RECTANGLE) &&
                    nc.bg_color != 0 && nc.bg_color != 0xFF1a1a2e) {
                    new_bg = nc.bg_color;
                    break;
                }
            }
            if (old_bg != new_bg) {
                start_transition(old_bg, new_bg,
                                0, 0, 100, 100,
                                0, 0, 100, 100, 0.4);
            }
        }

        m_old_commands = std::move(m_cached_commands);

        {
            std::lock_guard<std::mutex> lock(m_commands_mutex);
            m_cached_commands = std::move(commands);
        }

        HYPERION_LOG_INFO("In-process rendering complete: %zu commands", m_cached_commands.size());
    } catch (const std::exception& e) {
        HYPERION_LOG_ERROR("In-process rendering failed: %s", e.what());
        render_error_page(to_wstring(e.what()));
    }
}

void tab::render_error_page(const std::wstring& message) {
    std::vector<hre::render::render_command> commands;

    hre::render::render_command cmd;
    cmd.type = hre::render::command_type::RECTANGLE;
    cmd.x = 0; cmd.y = 0;
    cmd.width = 1920; cmd.height = 1080;
    cmd.bg_color = 0xFF1a1a2e;
    commands.push_back(cmd);

    cmd.type = hre::render::command_type::TEXT;
    cmd.x = 50; cmd.y = 50;
    cmd.width = 800; cmd.height = 40;
    cmd.text_content = L"Hyperion Browser";
    cmd.font_family = L"Segoe UI";
    cmd.font_size = 28;
    cmd.text_color = 0xFF00d9ff;
    commands.push_back(cmd);

    cmd.font_size = 16;
    cmd.text_color = 0xFFaa5555;
    cmd.y = 120;
    cmd.text_content = L"Error: " + message;
    commands.push_back(cmd);

    cmd.y = 160;
    cmd.text_color = 0xFFaaaaaa;
    cmd.text_content = L"Try navigating to native://home, native://demo, or native://test";
    commands.push_back(cmd);

    {
        std::lock_guard<std::mutex> lock(m_commands_mutex);
        m_cached_commands = std::move(commands);
    }
}

double tab::now_seconds() {
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

void tab::start_transition(uint32_t old_color, uint32_t new_color,
                            float old_x, float old_y, float old_w, float old_h,
                            float new_x, float new_y, float new_w, float new_h,
                            double duration) {
    m_transition.start_time = now_seconds();
    m_transition.duration = duration > 0 ? duration : 0.3;
    m_transition.old_color = old_color;
    m_transition.new_color = new_color;
    m_transition.old_x = old_x; m_transition.old_y = old_y;
    m_transition.old_w = old_w; m_transition.old_h = old_h;
    m_transition.new_x = new_x; m_transition.new_y = new_y;
    m_transition.new_w = new_w; m_transition.new_h = new_h;
    m_transition.active = true;
    m_transition_active = true;
}

void tab::update_transitions() {
    if (!m_transition_active || !m_transition.active) return;
    double elapsed = now_seconds() - m_transition.start_time;
    if (elapsed >= m_transition.duration) {
        m_transition.active = false;
        m_transition_active = false;
    }
}

void tab::clear_page_state() {
    m_script_engine.reset();
    m_layout_root.reset();
    m_layout_engine.reset();
    m_click_targets.clear();
    m_focused_element = nullptr;
    m_hovered_element = nullptr;
    m_image_cache.clear();
    m_scroll_top = 0;
    m_transition_active = false;
    m_transition.active = false;
    m_old_commands.clear();
}

void tab::navigate(const std::wstring& url) {
    clear_page_state();
    m_url = url;
    m_title = L"Loading...";
    m_progress = 0.4;
    m_use_native = true;

    hre::storage::session_storage::clear();

    // Push to history (remove any forward history first)
    m_history.erase(m_history.begin() + m_history_index + 1, m_history.end());
    m_history.push_back(url);
    m_history_index = m_history.size() - 1;
    // Cap history at 50 entries
    if (m_history.size() > 50) {
        m_history.erase(m_history.begin());
        m_history_index--;
    }

    HYPERION_LOG_INFO("Navigating in-process to: %s", to_utf8(url).c_str());
    render_page_in_process(url);

    if (m_title == L"Loading..." || m_title.empty()) {
        m_title = url;
    }
    m_progress = 1.0;

    if (on_title_changed) on_title_changed(m_title);
}

void tab::back() {
    if (m_history_index > 0) {
        m_history_index--;
        m_url = m_history[m_history_index];
        m_progress = 0.5;
        render_page_in_process(m_url);
        m_progress = 1.0;
    }
}

void tab::forward() {
    if (m_history_index + 1 < m_history.size()) {
        m_history_index++;
        m_url = m_history[m_history_index];
        m_progress = 0.5;
        render_page_in_process(m_url);
        m_progress = 1.0;
    }
}

void tab::reload() {
    navigate(m_url);
}

void tab::set_visible(bool visible) {
    m_visible = visible;
}

void tab::build_click_targets(const hre::layout::LayoutNode& node, float y_offset) {
    if (node.dom_node->type() == hre::dom::node_type::element) {
        auto* el = static_cast<const hre::dom::element*>(node.dom_node);
        std::wstring onclick = el->get_attribute(L"onclick");
        std::wstring href = el->get_attribute(L"href");
        if (!onclick.empty() || (!href.empty() && href.find(L"native://") == 0)) {
            hre::script::ClickTarget ct;
            ct.element = const_cast<hre::dom::element*>(el);
            ct.onclick_code = onclick;
            ct.x = node.content_box.x;
            ct.y = node.content_box.y;
            ct.width = node.content_box.width;
            ct.height = node.content_box.height;
            m_click_targets.push_back(ct);
        }
    }
    for (const auto& child : node.children) {
        build_click_targets(*child, y_offset);
    }
}

void tab::handle_mouse_down(int x, int y) {
    if (!m_script_engine || !m_layout_root) return;

    hre::dom::element* clicked_element = nullptr;
    for (const auto& t : m_click_targets) {
        float tx = static_cast<float>(t.x);
        float ty = static_cast<float>(t.y) - m_scroll_top + m_last_y_offset;
        float tw = static_cast<float>(t.width);
        float th = static_cast<float>(t.height);
        if (x >= tx && x <= tx + tw && y >= ty && y <= ty + th) {
            clicked_element = t.element;
            if (!t.onclick_code.empty()) {
                std::wstring on_click_js = L"(function(){" + t.onclick_code + L"})();";
                m_script_engine->execute(on_click_js);
            } else {
                std::wstring href = t.element->get_attribute(L"href");
                if (!href.empty() && href.find(L"native://") == 0) {
                    navigate(href);
                }
            }
            break;
        }
    }

    // Set focus on clicked input elements, handle submit buttons
    if (clicked_element) {
        std::wstring tag = clicked_element->tag_name();
        if (tag == L"input") {
            std::wstring input_type = clicked_element->get_attribute(L"type");
            if (input_type.empty() || input_type == L"text" || input_type == L"password" ||
                input_type == L"search" || input_type == L"email" || input_type == L"checkbox") {
                if (m_focused_element != clicked_element) {
                    invalidate_element_style(m_focused_element, m_focused_element == m_hovered_element, false);
                    m_focused_element = clicked_element;
                    invalidate_element_style(clicked_element, clicked_element == m_hovered_element, true);
                    regenerate_commands();
                }
                if (input_type == L"checkbox") {
                    std::wstring checked = clicked_element->get_attribute(L"checked");
                    if (checked == L"true" || checked == L"checked") {
                        clicked_element->set_attribute(L"checked", L"false");
                    } else {
                        clicked_element->set_attribute(L"checked", L"true");
                    }
                    regenerate_commands();
                }
            } else if (input_type == L"submit") {
                auto* form = find_form_parent(clicked_element);
                if (form) {
                    std::wstring onsubmit = form->get_attribute(L"onsubmit");
                    if (!onsubmit.empty()) {
                        m_script_engine->execute(L"(function(){" + onsubmit + L"})();");
                    } else {
                        std::wstring action = form->get_attribute(L"action");
                        if (!action.empty()) {
                            navigate(action);
                        }
                    }
                }
            } else if (input_type == L"reset") {
                std::function<void(hre::dom::node*)> reset_values = [&](hre::dom::node* n) {
                    if (n->type() == hre::dom::node_type::element) {
                        auto* e = static_cast<hre::dom::element*>(n);
                        if (e->tag_name() == L"input") {
                            std::wstring def = e->get_attribute(L"defaultValue");
                            if (!def.empty()) e->set_attribute(L"value", def);
                            else e->set_attribute(L"value", L"");
                        }
                    }
                    for (const auto& c : n->children()) reset_values(c.get());
                };
                auto* form = find_form_parent(clicked_element);
                if (form) reset_values(form);
                regenerate_commands();
            }
        } else if (tag == L"button") {
            std::wstring btn_type = clicked_element->get_attribute(L"type");
            if (btn_type.empty() || btn_type == L"submit") {
                auto* form = find_form_parent(clicked_element);
                if (form) {
                    std::wstring onsubmit = form->get_attribute(L"onsubmit");
                    if (!onsubmit.empty()) {
                        m_script_engine->execute(L"(function(){" + onsubmit + L"})();");
                    } else {
                        std::wstring action = form->get_attribute(L"action");
                        if (!action.empty()) {
                            navigate(action);
                        }
                    }
                }
            }
        } else if (tag == L"textarea") {
            if (m_focused_element != clicked_element) {
                invalidate_element_style(m_focused_element, m_focused_element == m_hovered_element, false);
                m_focused_element = clicked_element;
                invalidate_element_style(clicked_element, clicked_element == m_hovered_element, true);
                regenerate_commands();
            }
        } else {
            // Other element clicked: clear focus
            if (m_focused_element) {
                invalidate_element_style(m_focused_element, m_focused_element == m_hovered_element, false);
                m_focused_element = nullptr;
                regenerate_commands();
            }
        }
    } else {
        if (m_focused_element) {
            invalidate_element_style(m_focused_element, m_focused_element == m_hovered_element, false);
            m_focused_element = nullptr;
            regenerate_commands();
        }
    }
}

// Hit-test the layout tree to find the deepest element under cursor
static hre::dom::element* hit_test_element(const hre::layout::LayoutNode& node, float x, float y, float scroll_top, float y_offset) {
    if (node.dom_node->type() != hre::dom::node_type::element) return nullptr;
    if (node.style.display == L"none" || node.style.visibility == L"hidden") return nullptr;

    float nx = static_cast<float>(node.content_box.x);
    float ny = static_cast<float>(node.content_box.y) - scroll_top + y_offset;
    float nw = static_cast<float>(node.content_box.width);
    float nh = static_cast<float>(node.content_box.height);

    bool inside = (x >= nx && x <= nx + nw && y >= ny && y <= ny + nh);

    // Check children first to find innermost element
    hre::dom::element* inner = nullptr;
    for (const auto& child : node.children) {
        auto* result = hit_test_element(*child, x, y, scroll_top, y_offset);
        if (result) inner = result;
    }

    if (inside) {
        return inner ? inner : const_cast<hre::dom::element*>(static_cast<const hre::dom::element*>(node.dom_node));
    }
    return inner;
}

void tab::invalidate_element_style(hre::dom::element* el, bool is_hovered, bool is_focused) {
    if (!el || !m_layout_engine || !m_layout_root) return;
    m_layout_engine->update_style_for_node(m_layout_root, el, is_hovered, is_focused);
}

void tab::handle_mouse_move(int x, int y) {
    if (!m_layout_root) return;

    hre::dom::element* under_cursor = hit_test_element(*m_layout_root, static_cast<float>(x), static_cast<float>(y),
                                                        m_scroll_top, m_y_offset);

    if (under_cursor != m_hovered_element) {
        invalidate_element_style(m_hovered_element, false, m_hovered_element == m_focused_element);
        invalidate_element_style(under_cursor, true, under_cursor == m_focused_element);
        m_hovered_element = under_cursor;
        regenerate_commands();
    }
}

void tab::handle_mouse_wheel(float delta) {
    m_scroll_top -= delta;
    if (m_scroll_top < 0) m_scroll_top = 0;
    float max_scroll = static_cast<float>(m_bounds.bottom - m_bounds.top) * 2.0f;
    if (m_scroll_top > max_scroll) m_scroll_top = max_scroll;
}

void tab::handle_key_down(UINT vk, bool ctrl, bool alt, bool shift) {
    if (!m_focused_element || !m_script_engine) return;

    std::wstring key, code;
    switch (vk) {
        case VK_RETURN: key = L"Enter"; code = L"Enter"; break;
        case VK_BACK:   key = L"Backspace"; code = L"Backspace"; break;
        case VK_TAB:    key = L"Tab"; code = L"Tab"; break;
        case VK_ESCAPE: key = L"Escape"; code = L"Escape"; break;
        case VK_DELETE: key = L"Delete"; code = L"Delete"; break;
        case VK_LEFT:   key = L"ArrowLeft"; code = L"ArrowLeft"; break;
        case VK_RIGHT:  key = L"ArrowRight"; code = L"ArrowRight"; break;
        case VK_UP:     key = L"ArrowUp"; code = L"ArrowUp"; break;
        case VK_DOWN:   key = L"ArrowDown"; code = L"ArrowDown"; break;
        case VK_SPACE:  key = L" "; code = L"Space"; break;
        default:
            if (vk >= 0x30 && vk <= 0x5A) {
                key = std::wstring(1, (wchar_t)(shift ? vk : tolower(vk)));
                code = L"Key" + key;
            }
            break;
    }

    if (!key.empty()) {
        m_script_engine->trigger_keyboard_event(m_focused_element, L"keydown", key, code, ctrl, alt, shift);
    }

    // Handle text input editing for text inputs
    std::wstring tag = m_focused_element->tag_name();
    if (tag == L"input") {
        std::wstring itype = m_focused_element->get_attribute(L"type");
        if (itype.empty() || itype == L"text" || itype == L"password" || itype == L"search" || itype == L"email") {
            std::wstring val = m_focused_element->get_attribute(L"value");
            if (vk == VK_BACK && !val.empty()) {
                val.pop_back();
                m_focused_element->set_attribute(L"value", val);
                regenerate_commands();
            } else if (vk == VK_DELETE && !val.empty()) {
                val.pop_back();
                m_focused_element->set_attribute(L"value", val);
                regenerate_commands();
            } else if (vk == VK_RETURN) {
                auto* form = find_form_parent(m_focused_element);
                if (form) {
                    std::wstring onsubmit = form->get_attribute(L"onsubmit");
                    if (!onsubmit.empty()) {
                        m_script_engine->execute(L"(function(){" + onsubmit + L"})();");
                    } else {
                        std::wstring action = form->get_attribute(L"action");
                        if (!action.empty()) navigate(action);
                    }
                }
            }
        }
    } else if (tag == L"textarea") {
        std::wstring val = m_focused_element->get_attribute(L"value");
        if (vk == VK_BACK && !val.empty()) {
            val.pop_back();
            m_focused_element->set_attribute(L"value", val);
            regenerate_commands();
        } else if (vk == VK_DELETE && !val.empty()) {
            val.pop_back();
            m_focused_element->set_attribute(L"value", val);
            regenerate_commands();
        }
    }
}

void tab::handle_char(wchar_t c) {
    if (!m_focused_element) return;
    std::wstring tag = m_focused_element->tag_name();
    if (tag == L"input") {
        std::wstring itype = m_focused_element->get_attribute(L"type");
        if (itype.empty() || itype == L"text" || itype == L"password" || itype == L"search" || itype == L"email") {
            std::wstring val = m_focused_element->get_attribute(L"value");
            val += c;
            m_focused_element->set_attribute(L"value", val);
            regenerate_commands();
        }
    } else if (tag == L"textarea") {
        std::wstring val = m_focused_element->get_attribute(L"value");
        val += c;
        m_focused_element->set_attribute(L"value", val);
        regenerate_commands();
    }
}

hre::dom::element* tab::find_form_parent(hre::dom::element* el) {
    hre::dom::node* current = el;
    while (current) {
        if (current->type() == hre::dom::node_type::element) {
            auto* e = static_cast<hre::dom::element*>(current);
            if (e->tag_name() == L"form") return e;
        }
        current = current->parent();
    }
    return nullptr;
}

void tab::regenerate_commands() {
    if (!m_layout_root) return;
    std::vector<hre::render::render_command> commands;
    generate_commands_recursive(*m_layout_root, commands);
    {
        std::lock_guard<std::mutex> lock(m_commands_mutex);
        m_cached_commands = std::move(commands);
    }
}

#ifdef DrawText
#undef DrawText
#endif

void tab::render_native(ui::renderer& r, float y_offset) {
    std::lock_guard<std::mutex> lock(m_commands_mutex);
    if (m_cached_commands.empty()) return;
    if (!m_visible) return;

    RECT client_rect;
    if (!GetClientRect(m_parent_hwnd, &client_rect)) return;
    float width = (float)(client_rect.right - client_rect.left);
    float height = (float)(client_rect.bottom - client_rect.top);

    auto* context = r.context();
    if (!context) return;

    D2D1_MATRIX_3X2_F old_transform;
    context->GetTransform(&old_transform);

    context->PushAxisAlignedClip(D2D1::RectF(0, y_offset, width, height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    context->SetTransform(D2D1::Matrix3x2F::Translation(0, y_offset - m_scroll_top));

    auto to_d2d_color = [](uint32_t c) {
        float a = ((c >> 24) & 0xFF) / 255.0f;
        float r_val = ((c >> 16) & 0xFF) / 255.0f;
        float g_val = ((c >> 8) & 0xFF) / 255.0f;
        float b_val = (c & 0xFF) / 255.0f;
        return D2D1::ColorF(r_val, g_val, b_val, a);
    };

    // Cache brushes by color value to avoid per-frame allocations
    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>> brush_cache;

    auto get_brush = [&](uint32_t color_val) -> ID2D1SolidColorBrush* {
        auto it = brush_cache.find(color_val);
        if (it != brush_cache.end()) return it->second.Get();
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        if (SUCCEEDED(context->CreateSolidColorBrush(to_d2d_color(color_val), &brush))) {
            auto* ptr = brush.Get();
            brush_cache[color_val] = std::move(brush);
            return ptr;
        }
        return nullptr;
    };

    // Cache text format
    Microsoft::WRL::ComPtr<IDWriteTextFormat> cached_format;
    std::wstring cached_family;
    float cached_size = 0;

    auto get_format = [&](const std::wstring& family, float size) -> IDWriteTextFormat* {
        if (cached_format && cached_family == family && cached_size == size) {
            return cached_format.Get();
        }
        cached_format.Reset();
        cached_family = family;
        cached_size = size;
        r.write_factory()->CreateTextFormat(
            family.empty() ? L"Segoe UI" : family.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size > 0 ? size : 14.0f,
            L"en-us",
            &cached_format
        );
        return cached_format.Get();
    };

    update_transitions();

    float transition_t = 0.0f;
    if (m_transition_active && m_transition.active) {
        double elapsed = now_seconds() - m_transition.start_time;
        transition_t = (float)(elapsed / m_transition.duration);
        if (transition_t > 1.0f) transition_t = 1.0f;
        if (m_transition.timing_function == L"ease-in") {
            transition_t = transition_t * transition_t;
        } else if (m_transition.timing_function == L"ease-out") {
            transition_t = 1.0f - (1.0f - transition_t) * (1.0f - transition_t);
        } else if (m_transition.timing_function == L"ease-in-out") {
            transition_t = transition_t < 0.5f ? 2.0f * transition_t * transition_t
                                              : 1.0f - (float)pow(-2.0f * transition_t + 2.0f, 2.0f) / 2.0f;
        }
    }

    auto lerp_color = [](uint32_t a, uint32_t b, float t) -> uint32_t {
        auto lerp_byte = [](uint8_t ca, uint8_t cb, float t) -> uint8_t {
            return (uint8_t)((float)ca + ((float)cb - (float)ca) * t + 0.5f);
        };
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        uint8_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF, aa = (a >> 24) & 0xFF;
        uint8_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF, ba = (b >> 24) & 0xFF;
        return ((uint32_t)lerp_byte(aa, ba, t) << 24) |
               ((uint32_t)lerp_byte(ar, br, t) << 16) |
               ((uint32_t)lerp_byte(ag, bg, t) << 8) |
               (uint32_t)lerp_byte(ab, bb, t);
    };

    for (const auto& cmd : m_cached_commands) {
        D2D1_RECT_F rect = D2D1::RectF(cmd.x, cmd.y, cmd.x + cmd.width, cmd.y + cmd.height);

        uint32_t draw_color = cmd.bg_color;
        if (m_transition_active && m_transition.active && transition_t > 0.0f && transition_t < 1.0f) {
            draw_color = lerp_color(m_transition.old_color, m_transition.new_color, transition_t);
        }

        if (cmd.type == hre::render::command_type::SHADOW) {
            auto* brush = get_brush(cmd.shadow_color);
            if (brush) {
                D2D1_RECT_F shadow_rect = rect;
                shadow_rect.left += cmd.shadow_offset_x;
                shadow_rect.top += cmd.shadow_offset_y;
                shadow_rect.right += cmd.shadow_offset_x;
                shadow_rect.bottom += cmd.shadow_offset_y;
                context->FillRectangle(shadow_rect, brush);
            }

        } else if (cmd.type == hre::render::command_type::RECTANGLE) {
            if (cmd.border_width > 0) {
                auto* brush = get_brush(cmd.border_color);
                if (brush) context->DrawRectangle(rect, brush, cmd.border_width);
            } else if (cmd.bg_color2 != 0) {
                // Linear gradient
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stops;
                D2D1_GRADIENT_STOP grad_stops[2] = {
                    { 0.0f, to_d2d_color(cmd.bg_color) },
                    { 1.0f, to_d2d_color(cmd.bg_color2) }
                };
                if (SUCCEEDED(context->CreateGradientStopCollection(grad_stops, 2, &stops))) {
                    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> grad_brush;
                    if (SUCCEEDED(context->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(D2D1::Point2F(rect.left, rect.top),
                                                            D2D1::Point2F(rect.left, rect.bottom)),
                        stops.Get(), &grad_brush))) {
                        context->FillRectangle(rect, grad_brush.Get());
                    }
                }
            } else {
                auto* brush = get_brush(draw_color);
                if (brush) context->FillRectangle(rect, brush);
            }

        } else if (cmd.type == hre::render::command_type::ROUNDED_RECTANGLE) {
            auto rounded = D2D1::RoundedRect(rect, cmd.border_radius, cmd.border_radius);
            if (cmd.border_width > 0) {
                auto* brush = get_brush(cmd.border_color);
                if (brush) context->DrawRoundedRectangle(rounded, brush, cmd.border_width);
            } else if (cmd.bg_color2 != 0) {
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stops;
                D2D1_GRADIENT_STOP grad_stops[2] = {
                    { 0.0f, to_d2d_color(cmd.bg_color) },
                    { 1.0f, to_d2d_color(cmd.bg_color2) }
                };
                if (SUCCEEDED(context->CreateGradientStopCollection(grad_stops, 2, &stops))) {
                    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> grad_brush;
                    if (SUCCEEDED(context->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(D2D1::Point2F(rect.left, rect.top),
                                                            D2D1::Point2F(rect.left, rect.bottom)),
                        stops.Get(), &grad_brush))) {
                        context->FillRoundedRectangle(rounded, grad_brush.Get());
                    }
                }
            } else {
                auto* brush = get_brush(draw_color);
                if (brush) context->FillRoundedRectangle(rounded, brush);
            }

        } else if (cmd.type == hre::render::command_type::IMAGE) {
            if (!cmd.image_src.empty()) {
                Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
                auto cache_it = m_image_cache.find(cmd.image_src);
                if (cache_it != m_image_cache.end()) {
                    bitmap = cache_it->second;
                } else {
                    bitmap = hre::render::image_loader::load_from_url(cmd.image_src, context);
                    if (bitmap) {
                        m_image_cache[cmd.image_src] = bitmap;
                    }
                }
                if (bitmap) {
                    context->DrawBitmap(bitmap.Get(), rect);
                }
            }

        } else if (cmd.type == hre::render::command_type::TEXT) {
            auto* format = get_format(cmd.font_family, cmd.font_size);
            if (format) {
                auto* brush = get_brush(cmd.text_color);
                if (brush) {
                    context->DrawText(
                        cmd.text_content.c_str(),
                        (UINT32)cmd.text_content.length(),
                        format, &rect, brush
                    );
                }
            }
        }
    }

    context->PopAxisAlignedClip();
    context->SetTransform(old_transform);
}

} // namespace hyperion::browser
