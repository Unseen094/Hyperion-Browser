#include <hyperion/browser/application.hpp>
#include <hyperion/platform/logging.hpp>
#include <stdexcept>

namespace hyperion::browser {

// UI Layout Constants
constexpr int TOOLBAR_HEIGHT = 80;
constexpr int BOOKMARKS_HEIGHT = 28;
constexpr int TOTAL_UI_HEIGHT = TOOLBAR_HEIGHT + BOOKMARKS_HEIGHT;

application::application() {
    initialize();
}

application::~application() {
    shutdown();
}

void application::initialize() {
    HYPERION_LOG_INFO("Initializing Hyperion Application...");
    
    platform::window_config config;
    config.title = L"Hyperion Browser v0.1.0";
    
    m_main_window = std::make_unique<platform::window>(config);
    
    m_ui_renderer = std::make_unique<ui::renderer>();
    if (!m_ui_renderer->initialize(m_main_window->handle())) {
        HYPERION_LOG_ERROR("Failed to initialize UI renderer");
        throw std::runtime_error("UI renderer initialization failed");
    }

    m_toolbar = std::make_unique<ui::toolbar>();
    m_bookmarks_bar = std::make_unique<ui::bookmarks_bar>();
    m_tab_manager = std::make_unique<tab_manager>();

    RECT bounds;
    GetClientRect(m_main_window->handle(), &bounds);
    m_toolbar->handle_resize(bounds.right - bounds.left, bounds.bottom - bounds.top);
    m_bookmarks_bar->handle_resize(bounds.right - bounds.left, bounds.bottom - bounds.top);

    HYPERION_LOG_INFO("Creating initial tab...");

    auto* initial_tab = m_tab_manager->create_tab(L"native://home");

    if (initial_tab) {
        HYPERION_LOG_INFO("Initializing webview...");
        initial_tab->initialize_webview(m_main_window->handle());
        bounds.top += TOTAL_UI_HEIGHT;
        initial_tab->resize_webview(bounds);
        HYPERION_LOG_INFO("Tab initialized successfully - showing demo page");
    } else {
        HYPERION_LOG_ERROR("Failed to create initial tab");
    }
    GetClientRect(m_main_window->handle(), &bounds);
    m_toolbar->handle_resize(bounds.right - bounds.left, bounds.bottom - bounds.top);
    m_bookmarks_bar->handle_resize(bounds.right - bounds.left, bounds.bottom - bounds.top);

    // Note: Skipping webview resize - renderer disabled for debugging

    m_main_window->on_resize([this](int width, int height) {
        if (m_ui_renderer) m_ui_renderer->resize(width, height);
        if (m_toolbar) m_toolbar->handle_resize(width, height);
        if (m_bookmarks_bar) m_bookmarks_bar->handle_resize(width, height);
        if (m_tab_manager && m_tab_manager->active_tab()) {
            RECT r = { 0, TOTAL_UI_HEIGHT, width, height };
            m_tab_manager->active_tab()->resize_webview(r);
        }
    });

    m_main_window->on_key([this](UINT key, bool is_down) {
        if (!is_down) return;

        if (key == VK_RETURN) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->navigate(m_toolbar->address());
            }
        } else if (key == VK_F5) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->reload();
            }
        } else if (key == VK_LEFT && (GetKeyState(VK_MENU) & 0x8000)) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->back();
            }
        } else if (key == VK_RIGHT && (GetKeyState(VK_MENU) & 0x8000)) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->forward();
            }
        }
    });

    m_main_window->on_char([this](wchar_t c) {
        if (m_toolbar) {
            m_toolbar->handle_char(c);
        }
    });

    m_main_window->on_mouse([this](int x, int y, bool is_down) {
        if (is_down) {
            if (y < TOOLBAR_HEIGHT) {
                if (m_toolbar) m_toolbar->handle_mouse_down(x, y);
            } else if (y < TOTAL_UI_HEIGHT) {
                if (m_bookmarks_bar) m_bookmarks_bar->handle_mouse_down(x, y - TOOLBAR_HEIGHT);
            } else {
                if (m_tab_manager && m_tab_manager->active_tab()) {
                    m_tab_manager->active_tab()->handle_mouse_down(x, y);
                    InvalidateRect(m_main_window->handle(), nullptr, FALSE);
                }
            }
        }
    });

    m_main_window->on_mouse_wheel([this](float delta) {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->handle_mouse_wheel(delta);
            InvalidateRect(m_main_window->handle(), nullptr, FALSE);
        }
    });

    m_toolbar->on_add_bookmark_click([this](const std::wstring& title, const std::wstring& url) {
        if (m_bookmarks_bar) {
            m_bookmarks_bar->add_bookmark(title, url);
        }
    });

    m_bookmarks_bar->on_bookmark_click([this](const std::wstring& url) {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->navigate(url);
        }
    });

    m_toolbar->on_tab_click([this](size_t index) {
        if (m_tab_manager) {
            m_tab_manager->set_active_tab(index);
            // Re-sync UI
        }
    });

    m_toolbar->on_tab_close([this](size_t index) {
        if (m_tab_manager) {
            m_tab_manager->close_tab(index);
            
            // If no tabs left, shut down
            if (m_tab_manager->tabs().empty()) {
                m_running = false;
            }
        }
    });

    m_toolbar->on_new_tab_click([this]() {
        if (m_tab_manager) {
            auto* new_tab = m_tab_manager->create_tab(L"native://home");
            new_tab->initialize_webview(m_main_window->handle());

            RECT bounds;
            GetClientRect(m_main_window->handle(), &bounds);
            bounds.top += TOTAL_UI_HEIGHT;
            new_tab->resize_webview(bounds);
        }
    });

    m_toolbar->on_back_click([this]() {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->back();
        }
    });

    m_toolbar->on_forward_click([this]() {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->forward();
        }
    });

    m_toolbar->on_refresh_click([this]() {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->reload();
        }
    });

    m_toolbar->on_home_click([this]() {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->navigate(L"native://home");
        }
    });
}

void application::shutdown() {
    HYPERION_LOG_INFO("Shutting down Hyperion Application...");
    m_main_window.reset();
}

int application::run() {
    m_running = true;
    m_main_window->show();

    HYPERION_LOG_INFO("Entering main event loop. m_running = {}", m_running);

    while (m_running) {
        // Process Windows messages with proper waiting
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                HYPERION_LOG_INFO("Received WM_QUIT");
                m_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // No messages - wait for one instead of busy looping
            // In some environments WaitMessage might return immediately
            WaitMessage();
        }
        
        if (!m_running) {
            HYPERION_LOG_INFO("m_running is now false, exiting loop");
        }
        
        // Sync URL to toolbar
        if (m_tab_manager) {
            std::vector<ui::tab_ui_info> tab_infos;
            auto active_index = m_tab_manager->active_index();
            const auto& tabs = m_tab_manager->tabs();

            for (size_t i = 0; i < tabs.size(); ++i) {
                tab_infos.push_back({ tabs[i]->title(), i == active_index, {0, 0, 0, 0} });
            }
            m_toolbar->set_tabs(tab_infos);

            if (m_tab_manager->active_tab()) {
                auto* active = m_tab_manager->active_tab();
                const auto& tab_url = active->url();
                if (m_toolbar->address() != tab_url) {
                    m_toolbar->set_address(tab_url);
                }
                
                m_toolbar->set_navigation_state(active->can_go_back(), active->can_go_forward());
                m_toolbar->set_progress(active->progress());
            }
        }

        // Render UI
        m_ui_renderer->begin_draw();
        m_ui_renderer->clear(D2D1::ColorF(0.98f, 0.98f, 1.0f, 1.0f)); // Pure white/light day mode background
        
        if (m_toolbar) m_toolbar->render(*m_ui_renderer);
        
        if (m_bookmarks_bar) {
            // Apply a transform to render the bookmarks bar below the toolbar
            auto* context = m_ui_renderer->context();
            context->SetTransform(D2D1::Matrix3x2F::Translation(0, TOOLBAR_HEIGHT));
            m_bookmarks_bar->render(*m_ui_renderer);
            context->SetTransform(D2D1::Matrix3x2F::Identity());
        }

        // Render Native Content if active
        if (m_tab_manager && m_tab_manager->active_tab()) {
            auto* active = m_tab_manager->active_tab();
            if (active->is_native()) {
                active->render_native(*m_ui_renderer);
            }
        }
        
        m_ui_renderer->end_draw();
    }

    return 0;
}

} // namespace hyperion::browser
