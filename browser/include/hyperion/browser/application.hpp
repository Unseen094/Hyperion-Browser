#pragma once

#include <hyperion/platform/window.hpp>
#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/toolbar.hpp>
#include <hyperion/ui/bookmarks_bar.hpp>
#include <hyperion/browser/tab_manager.hpp>
#include <memory>
#include <vector>

namespace hyperion::browser {

class application {
public:
    application();
    ~application();

    int run();

private:
    void initialize();
    void shutdown();

    std::unique_ptr<platform::window> m_main_window;
    std::unique_ptr<ui::renderer> m_ui_renderer;
    std::unique_ptr<ui::toolbar> m_toolbar;
    std::unique_ptr<ui::bookmarks_bar> m_bookmarks_bar;
    std::unique_ptr<tab_manager> m_tab_manager;
    bool m_running = false;
};

} // namespace hyperion::browser
