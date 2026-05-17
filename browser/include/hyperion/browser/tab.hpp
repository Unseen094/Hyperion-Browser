#pragma once

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <windows.h>
#include <wrl/client.h>

// HRE IPC & Rendering Includes
#include <hyperion/platform/ipc/pipe_channel.hpp>
#include <hre/render/render_command.hpp>

namespace hyperion::ui { class renderer; }

namespace hyperion::browser {

class tab {
public:
    tab(const std::wstring& initial_url);
    ~tab();

    const std::wstring& url() const { return m_url; }
    const std::wstring& title() const { return m_title; }

    void navigate(const std::wstring& url);
    void back();
    void forward();
    void reload();

    void set_visible(bool visible);

    bool can_go_back() const { return m_can_go_back; }
    bool can_go_forward() const { return m_can_go_forward; }
    double progress() const { return m_progress; }

    void initialize_webview(HWND parent_hwnd);
    void resize_webview(const RECT& bounds);

    void render_native(ui::renderer& r);
    void toggle_native(bool use_native) { m_use_native = use_native; }
    bool is_native() const { return m_use_native; }
    void handle_mouse_wheel(float delta);
    void handle_mouse_down(int x, int y);

private:
    std::wstring m_url;
    std::wstring m_title;
    HWND m_parent_hwnd = nullptr;

    bool m_can_go_back = false;
    bool m_can_go_forward = false;
    double m_progress = 0.0;

    bool m_use_native = false;
    float m_scroll_top = 0.0f;

    // Multiprocess IPC Members
    std::wstring m_pipe_name;
    std::unique_ptr<hyperion::platform::ipc::pipe_channel> m_ipc_channel;
    std::thread m_reader_thread;
    std::mutex m_commands_mutex;
    std::vector<hre::render::render_command> m_cached_commands;
    bool m_connected = false;
};

} // namespace hyperion::browser
