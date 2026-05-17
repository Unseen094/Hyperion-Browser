#include "../../engine/include/hre/dom/node.hpp"
#include "../../engine/include/hre/parser/html_parser.hpp"
#include "../../engine/include/hre/layout/layout_engine.hpp"
#include "../../engine/include/hre/css/css_parser.hpp"
#include "../../engine/include/hre/style/style_engine.hpp"
#include "../../engine/include/hre/script/script_engine.hpp"
#include "../../engine/include/hre/net/request.hpp"
#include "../../engine/include/hre/render/render_command.hpp"
#include "../../platform/include/hyperion/platform/ipc/pipe_channel.hpp"
#include "../../platform/include/hyperion/platform/logging.hpp"
#include "../../ui/include/hyperion/ui/renderer.hpp"
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <codecvt>
#include <locale>
#include <sstream>

using namespace hyperion::platform::ipc;

// Helper: UTF-8 string -> wstring
std::wstring to_wstring(const std::string& str) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
#pragma warning(pop)
}

// Helper: wstring -> UTF-8 string
std::string to_utf8(const std::wstring& ws) {
#pragma warning(push)
#pragma warning(disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws);
#pragma warning(pop)
}

// Helper: CSS Color string -> uint32_t (0xAARRGGBB)
uint32_t css_color_to_uint32(const std::wstring& color) {
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
    
    // Handle hex colors #RGB or #RRGGBB
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

    return 0xFF333333; // Default dark gray
}

// Convert computed layout node tree to a flat list of render commands
void generate_commands_recursive(const hre::layout::layout_node& node, std::vector<hre::render::render_command>& out_commands) {
    if (node.dom_node->type() == hre::dom::node_type::element) {
        auto* el = static_cast<hre::dom::element*>(node.dom_node);
        const auto& s = hre::style::g_computed_styles[node.dom_node];

        if (s.display == L"none" || s.visibility == L"hidden") return;

        // 1. Box Shadow
        if (s.box_shadow_blur > 0) {
            hre::render::render_command cmd;
            cmd.type = hre::render::command_type::SHADOW;
            cmd.x = node.dimensions.x;
            cmd.y = node.dimensions.y;
            cmd.width = node.dimensions.width;
            cmd.height = node.dimensions.height;
            cmd.shadow_offset_x = s.box_shadow_offset_x;
            cmd.shadow_offset_y = s.box_shadow_offset_y;
            cmd.shadow_color = css_color_to_uint32(s.box_shadow_color);
            cmd.shadow_blur = s.box_shadow_blur;
            out_commands.push_back(cmd);
        }

        // 2. Background
        if (s.background_color != L"transparent") {
            hre::render::render_command cmd;
            cmd.type = s.border_radius > 0 ? hre::render::command_type::ROUNDED_RECTANGLE : hre::render::command_type::RECTANGLE;
            cmd.x = node.dimensions.x;
            cmd.y = node.dimensions.y;
            cmd.width = node.dimensions.width;
            cmd.height = node.dimensions.height;
            cmd.bg_color = css_color_to_uint32(s.background_color);
            cmd.border_radius = s.border_radius;
            out_commands.push_back(cmd);
        }

        // 3. Image
        if (el->tag_name() == L"img") {
            std::wstring src = el->get_attribute(L"src");
            if (!src.empty()) {
                hre::render::render_command cmd;
                cmd.type = hre::render::command_type::IMAGE;
                cmd.x = node.dimensions.x;
                cmd.y = node.dimensions.y;
                cmd.width = node.dimensions.width;
                cmd.height = node.dimensions.height;
                cmd.image_src = src;
                out_commands.push_back(cmd);
            }
        }

        // 4. Border
        float bw = s.border_top; // Simplified
        if (bw > 0 && s.border_color != L"transparent") {
            hre::render::render_command cmd;
            cmd.type = s.border_radius > 0 ? hre::render::command_type::ROUNDED_RECTANGLE : hre::render::command_type::RECTANGLE;
            cmd.x = node.dimensions.x;
            cmd.y = node.dimensions.y;
            cmd.width = node.dimensions.width;
            cmd.height = node.dimensions.height;
            cmd.bg_color = 0; // Transparent fill
            cmd.border_color = css_color_to_uint32(s.border_color);
            cmd.border_width = bw;
            cmd.border_radius = s.border_radius;
            out_commands.push_back(cmd);
        }

    } else if (node.dom_node->type() == hre::dom::node_type::text) {
        auto* text_node = static_cast<hre::dom::text_node*>(node.dom_node);
        auto& parent_style = hre::style::g_computed_styles[node.dom_node->parent()];

        hre::render::render_command cmd;
        cmd.type = hre::render::command_type::TEXT;
        cmd.x = node.dimensions.x;
        cmd.y = node.dimensions.y;
        cmd.width = node.dimensions.width;
        cmd.height = node.dimensions.height;
        cmd.text_content = text_node->text();
        cmd.font_family = parent_style.font_family.empty() ? L"Segoe UI" : parent_style.font_family;
        cmd.font_size = parent_style.font_size > 0 ? parent_style.font_size : 14.0f;
        cmd.text_color = css_color_to_uint32(parent_style.color);
        out_commands.push_back(cmd);
    }

    // Process children
    for (const auto& child : node.children) {
        generate_commands_recursive(child, out_commands);
    }
}

int main(int argc, char* argv[]) {
    // 1. Initialize COM for DirectWrite & Networking
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return -1;

    std::wstring pipe_name;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--pipe=") == 0) {
            pipe_name = to_wstring(arg.substr(7));
        }
    }

    if (pipe_name.empty()) {
        CoUninitialize();
        return -1;
    }

    // 2. Connect to Named Pipe
    pipe_channel channel;
    if (!channel.connect(pipe_name)) {
        CoUninitialize();
        return -1;
    }

    HYPERION_LOG_INFO("Renderer process successfully connected to pipe: {}", to_utf8(pipe_name));

    // 3. Setup headless DWrite/D2D engine context for layouts
    hyperion::ui::renderer ui_renderer;
    // We pass nullptr to initialize DirectWrite only, safe for sandboxed process
    ui_renderer.initialize(nullptr);

    // Renderer state
    std::unique_ptr<hre::dom::document> document;
    std::optional<hre::layout::layout_node> layout_root;
    std::wstring current_url;
    float current_width = 1000.0f; // Default viewport width

    // Message handler lambdas
    auto recompute_and_send_frame = [&]() {
        if (!document) return;

        // Compute Layout
        hre::layout::layout_engine layout_eng;
        layout_root = layout_eng.compute_layout(document.get(), current_width, ui_renderer);

        // Traverse Layout and generate render commands
        std::vector<hre::render::render_command> commands;
        if (layout_root) {
            generate_commands_recursive(*layout_root, commands);
        }

        // Serialize and Send frame
        message response;
        response.header.type = message_type::RENDER_FRAME;
        response.payload = hre::render::serialize_frame(commands);
        response.header.payload_size = (uint32_t)response.payload.size();

        channel.send(response);
        HYPERION_LOG_INFO("Renderer successfully dispatched serialized RENDER_FRAME ({} bytes)", response.header.payload_size);
    };

    // 4. Main Event/IPC Loop
    message msg;
    while (channel.receive(msg)) {
        if (msg.header.type == message_type::NAVIGATE) {
            std::string url_str(msg.payload.begin(), msg.payload.end());
            current_url = to_wstring(url_str);
            HYPERION_LOG_INFO("Renderer received NAVIGATE command: {}", url_str);

            std::wstring html_content;
            if (current_url.find(L"native://home") == 0) {
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
            } else if (current_url.find(L"native://demo") == 0) {
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
                    L"  <script>"
                    L"    var arr = [1, 2, 3, 4, 5];"
                    L"    var sum = arr.reduce(function(a, b) { return a + b; }, 0);"
                    L"    document.getElementById('array-test').innerText = 'Array sum: ' + sum;"
                    L"    var str = 'Hello, World!';"
                    L"    document.getElementById('string-test').innerText = 'Upper: ' + str.toUpperCase();"
                    L"  </script>"
                    L"</body>"
                    L"</html>";
            } else if (current_url.find(L"native://test") == 0) {
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
            } else {
                html_content = hre::net::request::fetch(current_url);
            }

            // Parse via Rust HTML parser static library
            hre::parser::html_parser parser(to_utf8(html_content));
            document = parser.parse();

            // Default stylesheet parsing
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

            hre::css::css_parser css_p(default_css);
            auto stylesheet = css_p.parse();

            // Apply Styles
            hre::style::style_engine style_eng;
            style_eng.apply_styles(document.get(), stylesheet);

            // Execute scripting
            hre::script::script_engine script_eng(document.get());
            for (const auto& script_source : document->scripts()) {
                script_eng.execute(script_source);
            }

            // Recompute layout and transmit the rendered frame
            recompute_and_send_frame();

            // Send DOM_READY to notify the host
            message ready_msg;
            ready_msg.header.type = message_type::DOM_READY;
            ready_msg.header.payload_size = 0;
            channel.send(ready_msg);

        } else if (msg.header.type == message_type::INPUT_EVENT) {
            // Unpack mouse click event (float x, float y)
            if (msg.payload.size() >= sizeof(float) * 2 && layout_root) {
                float x = 0.0f;
                float y = 0.0f;
                std::memcpy(&x, msg.payload.data(), sizeof(float));
                std::memcpy(&y, msg.payload.data() + sizeof(float), sizeof(float));

                HYPERION_LOG_INFO("Renderer hit-testing click at ({:.1f}, {:.1f})", x, y);

                hre::layout::layout_engine layout_eng;
                auto* hit = layout_eng.hit_test(*layout_root, x, y);
                if (hit && hit->dom_node) {
                    HYPERION_LOG_INFO("Renderer hit-tested DOM element! Triggering click...");
                    hre::script::script_engine script_eng(document.get());
                    script_eng.trigger_event(hit->dom_node, L"click");

                    // Re-render and send frame in case JS changed styles/contents
                    recompute_and_send_frame();
                }
            }
        }
    }

    channel.close();
    CoUninitialize();
    return 0;
}
