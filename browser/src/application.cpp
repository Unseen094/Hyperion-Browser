#include <hyperion/browser/application.hpp>
#include <hyperion/platform/logging.hpp>
#include <stdexcept>

namespace hyperion::browser {

application::application() {
    initialize();
}

application::~application() {
    shutdown();
}

void application::initialize() {
    HYPERION_LOG_INFO("Initializing Hyperion Application...");

    platform::window_config config;
    config.title = L"Hyperion Browser v0.2.0 - Modern UI";

    m_main_window = std::make_unique<platform::window>(config);

    m_ui_renderer = std::make_unique<ui::renderer>();
    if (!m_ui_renderer->initialize(m_main_window->handle())) {
        HYPERION_LOG_ERROR("Failed to initialize UI renderer");
        throw std::runtime_error("UI renderer initialization failed");
    }

    m_toolbar = std::make_unique<ui::toolbar>();
    m_toolbar->initialize(m_main_window->handle(), *m_ui_renderer);
    m_tab_manager = std::make_unique<tab_manager>();

    RECT bounds;
    GetClientRect(m_main_window->handle(), &bounds);
    m_toolbar->handle_resize(bounds.right - bounds.left, bounds.bottom - bounds.top);

    HYPERION_LOG_INFO("Creating initial tab...");

    auto* initial_tab = m_tab_manager->create_tab(L"native://home");

    if (initial_tab) {
        HYPERION_LOG_INFO("Initializing webview...");
        initial_tab->initialize_webview(m_main_window->handle());
        initial_tab->navigate(L"native://home");
        initial_tab->on_title_changed = [this](const std::wstring& title) {
            update_window_title();
        };
        bounds.top += m_toolbar->total_ui_height();
        initial_tab->resize_webview(bounds);
        HYPERION_LOG_INFO("Tab initialized successfully - showing UI");
    } else {
        HYPERION_LOG_ERROR("Failed to create initial tab");
    }

    GetClientRect(m_main_window->handle(), &bounds);
    m_toolbar->handle_resize(bounds.right - bounds.left, bounds.bottom - bounds.top);

    // WM_NCHITTEST handler: return HTCAPTION for toolbar dead zones (enables window dragging),
    // HTCLIENT for interactive areas, HT* for resize edges
    m_main_window->on_nc_hit_test([this](POINT pt_screen) -> LRESULT {
        RECT win_rect;
        GetWindowRect(m_main_window->handle(), &win_rect);
        int w = win_rect.right - win_rect.left;
        int h = win_rect.bottom - win_rect.top;

        // Resize edge detection (6px border)
        const int edge = 6;
        bool top = pt_screen.y - win_rect.top < edge;
        bool bottom = win_rect.bottom - pt_screen.y < edge;
        bool left = pt_screen.x - win_rect.left < edge;
        bool right = win_rect.right - pt_screen.x < edge;

        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;

        // Toolbar area: HTCAPTION for dead zones, HTCLIENT for interactive controls
        POINT pt_client = pt_screen;
        ScreenToClient(m_main_window->handle(), &pt_client);
        if (pt_client.y >= 0 && pt_client.y < m_toolbar->total_ui_height()) {
            return m_toolbar->is_point_interactive(pt_client.x, pt_client.y)
                ? HTCLIENT : HTCAPTION;
        }
        return HTCLIENT;
    });

    m_main_window->on_resize([this](int width, int height) {
        if (m_ui_renderer) m_ui_renderer->resize(width, height);
        if (m_toolbar) m_toolbar->handle_resize(width, height);
        if (m_tab_manager && m_tab_manager->active_tab()) {
            RECT r = { 0, m_toolbar->total_ui_height(), width, height };
            m_tab_manager->active_tab()->resize_webview(r);
        }
    });

    m_main_window->on_key([this](UINT key, bool is_down) {
        if (!is_down) return;

        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (key == VK_RETURN) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->navigate(m_toolbar->address());
            }
        } else if (key == VK_F5) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->reload();
            }
        } else if (key == VK_LEFT && alt) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->back();
            }
        } else if (key == VK_RIGHT && alt) {
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->forward();
            }
        } else if (key == VK_ESCAPE) {
            if (m_toolbar) {
                m_toolbar->omnibox().set_focused(false);
            }
        } else if (key == 'T' && ctrl && !shift) {
            if (m_tab_manager) {
                auto* t = m_tab_manager->create_tab(L"native://home");
                if (t) {
                    t->initialize_webview(m_main_window->handle());
                    t->navigate(L"native://home");
                    t->on_title_changed = [this](const std::wstring& title) {
                        update_window_title();
                    };
                    RECT r; GetClientRect(m_main_window->handle(), &r);
                    r.top += m_toolbar->total_ui_height();
                    t->resize_webview(r);
                    update_window_title();
                }
            }
        } else if (key == 'W' && ctrl) {
            close_current_tab();
        } else if (key == VK_TAB && ctrl && !shift) {
            cycle_tab(1);
        } else if (key == VK_TAB && ctrl && shift) {
            cycle_tab(-1);
        } else if (key == 'L' && ctrl) {
            m_toolbar->omnibox().set_focused(true);
        } else if (key == 'D' && ctrl) {
            // Ctrl+D: bookmark current page
            if (m_tab_manager && m_tab_manager->active_tab()) {
                auto& tab = *m_tab_manager->active_tab();
                m_toolbar->bookmarks_bar().add_bookmark(tab.title(), tab.url());
            }
        } else if (key == 'H' && ctrl) {
            // Ctrl+H: go home
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->navigate(L"native://home");
            }
        } else {
            if (m_toolbar) m_toolbar->handle_key_down(key);
            // Forward keyboard events to active tab for DOM dispatch
            if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->handle_key_down(key, ctrl, alt, shift);
            }
        }
    });

    m_main_window->on_char([this](wchar_t c) {
        if (m_toolbar) m_toolbar->handle_char(c);
        // Forward char to active tab for text input in forms
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->handle_char(c);
        }
    });

    m_main_window->on_mouse([this](int x, int y, bool is_down) {
        int ui_h = m_toolbar->total_ui_height();
        if (is_down) {
            if (y < ui_h) {
                if (m_toolbar) m_toolbar->handle_mouse_down(x, y);
            } else {
                if (m_tab_manager && m_tab_manager->active_tab()) {
                    m_tab_manager->active_tab()->handle_mouse_down(x, y);
                    InvalidateRect(m_main_window->handle(), nullptr, FALSE);
                }
            }
        } else {
            if (y < ui_h) {
                if (m_toolbar) m_toolbar->handle_mouse_move(x, y);
            } else if (m_tab_manager && m_tab_manager->active_tab()) {
                m_tab_manager->active_tab()->handle_mouse_move(x, y);
                InvalidateRect(m_main_window->handle(), nullptr, FALSE);
            }
        }
    });

    m_main_window->on_mouse_wheel([this](float delta) {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->handle_mouse_wheel(delta);
            InvalidateRect(m_main_window->handle(), nullptr, FALSE);
        }
    });

    // Focus omnibox when window activates (Alt+Tab, click to focus)
    m_main_window->on_activate([this](bool focused) {
        if (focused && m_toolbar) {
            // Don't force-focus, just ensure the cursor is ready
        }
    });

    // Toolbar bookmark integration
    m_toolbar->on_add_bookmark_click([this](const std::wstring& title, const std::wstring& url) {
        if (m_toolbar->bookmarks_bar().has_bookmark(url)) {
            m_toolbar->bookmarks_bar().remove_bookmark(url);
        } else {
            m_toolbar->bookmarks_bar().add_bookmark(title, url);
        }
    });

    m_toolbar->bookmarks_bar().on_bookmark_click([this](const std::wstring& url) {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->navigate(url);
        }
    });

    // Omnibox enter handler with URL validation
    m_toolbar->on_omnibox_enter([this](const std::wstring& text) {
        if (m_tab_manager && m_tab_manager->active_tab()) {
            std::wstring url = validate_url(text);
            m_tab_manager->active_tab()->navigate(url);
        }
    });

    // Omnibox right-side icon callbacks
    m_toolbar->omnibox().on_extensions_click([this]() {
        // Extensions placeholder - show message on dashboard
        HYPERION_LOG_INFO("Extensions clicked - not yet implemented");
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->navigate(L"native://home");
        }
    });

    m_toolbar->omnibox().on_profile_click([this]() {
        // Profile/Sync placeholder
        HYPERION_LOG_INFO("Profile clicked - not yet implemented");
    });

    m_toolbar->omnibox().on_settings_click([this]() {
        // Settings placeholder - navigate to settings page
        HYPERION_LOG_INFO("Settings clicked");
        if (m_tab_manager && m_tab_manager->active_tab()) {
            m_tab_manager->active_tab()->navigate(L"native://settings");
        }
    });

    m_toolbar->omnibox().on_ai_click([this]() {
        m_toolbar->toggle_ai_sidebar();
    });

    m_toolbar->omnibox().on_downloads_click([this]() {
        // Downloads placeholder
        HYPERION_LOG_INFO("Downloads clicked - not yet implemented");
    });

    m_toolbar->on_tab_click([this](size_t index) {
        if (m_tab_manager) {
            m_tab_manager->set_active_tab(index);
            update_window_title();
        }
    });

    m_toolbar->on_tab_close([this](size_t index) {
        if (m_tab_manager) {
            m_tab_manager->close_tab(index);
            if (m_tab_manager->tabs().empty()) {
                m_running = false;
            } else {
                update_window_title();
            }
        }
    });

    m_toolbar->on_new_tab_click([this]() {
        if (m_tab_manager) {
            auto* new_tab = m_tab_manager->create_tab(L"native://home");
            if (new_tab) {
                new_tab->initialize_webview(m_main_window->handle());
                new_tab->navigate(L"native://home");
                new_tab->on_title_changed = [this](const std::wstring& title) {
                    update_window_title();
                };
                RECT bounds;
                GetClientRect(m_main_window->handle(), &bounds);
                bounds.top += m_toolbar->total_ui_height();
                new_tab->resize_webview(bounds);
                update_window_title();
            }
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

void application::close_current_tab() {
    if (!m_tab_manager) return;
    auto idx = m_tab_manager->active_index();
    m_tab_manager->close_tab(idx);
    if (m_tab_manager->tabs().empty()) {
        m_running = false;
    } else {
        update_window_title();
    }
}

void application::cycle_tab(int dir) {
    if (!m_tab_manager || m_tab_manager->tabs().empty()) return;
    size_t count = m_tab_manager->tabs().size();
    size_t next = (m_tab_manager->active_index() + count + dir) % count;
    m_tab_manager->set_active_tab(next);
    update_window_title();
}

void application::update_window_title() {
    if (!m_main_window || !m_tab_manager || !m_tab_manager->active_tab()) return;
    const auto& title = m_tab_manager->active_tab()->title();
    SetWindowTextW(m_main_window->handle(),
                   (title + L" - Hyperion Browser").c_str());
}

std::wstring application::validate_url(const std::wstring& input) {
    std::wstring trimmed = input;
    // Trim whitespace
    while (!trimmed.empty() && (trimmed[0] == L' ' || trimmed[0] == L'\t')) trimmed.erase(0, 1);
    while (!trimmed.empty() && (trimmed.back() == L' ' || trimmed.back() == L'\t')) trimmed.pop_back();

    if (trimmed.empty()) return L"native://home";

    // Check if it's already a valid URL with protocol
    if (trimmed.find(L"://") != std::wstring::npos) {
        return trimmed;
    }

    // Check if it looks like a domain (contains a dot and no spaces)
    bool has_dot = trimmed.find(L'.') != std::wstring::npos;
    bool has_space = trimmed.find(L' ') != std::wstring::npos;

    if (has_dot && !has_space) {
        return L"https://" + trimmed;
    }

    // Default: search query
    // URL encode the query
    std::wstring encoded;
    for (wchar_t c : trimmed) {
        if (c <= 32 || c == 127 || c == L'"' || c == L'%' || c == L'&' ||
            c == L'+' || c == L',' || c == L'/' || c == L':' ||
            c == L';' || c == L'<' || c == L'>' || c == L'?' ||
            c == L'@' || c == L'[' || c == L'\\' || c == L']' ||
            c == L'^' || c == L'`' || c == L'{' || c == L'|' || c == L'}') {
            wchar_t buf[4];
            swprintf(buf, 4, L"%%%02X", (unsigned)(unsigned char)c);
            encoded += buf;
        } else {
            encoded += c;
        }
    }
    return L"https://www.google.com/search?q=" + encoded;
}

void application::shutdown() {
    HYPERION_LOG_INFO("Shutting down Hyperion Application...");

    // 1. Release UI subsystems (toolbar uses renderer's D2D context)
    m_toolbar.reset();
    m_ui_renderer.reset();

    // 2. Close all tabs (release webview resources)
    m_tab_manager.reset();

    // 3. Destroy the window last (HWND must outlive D2D/swap-chain usage)
    m_main_window.reset();
}

void application::sync_toolbar_state() {
    if (!m_tab_manager || !m_toolbar) return;

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

int application::run() {
    m_running = true;
    m_main_window->show();

    HYPERION_LOG_INFO("Entering main event loop");

    while (m_running) {
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Use a short sleep instead of WaitMessage so animations keep ticking
            bool has_animations = m_toolbar && (
                m_toolbar->is_dashboard_showing() ||
                (m_toolbar->sidebar_open() && m_toolbar->sidebar_anim_progress() < 1.0f)
            );
            if (!has_animations && m_tab_manager && m_tab_manager->active_tab()) {
                has_animations = m_tab_manager->active_tab()->is_transitioning();
            }
            if (!has_animations) {
                WaitMessage();
            } else {
                Sleep(8); // ~120fps for smooth animation
            }
        }

        sync_toolbar_state();

        m_ui_renderer->begin_draw();
        auto bg = ui::theme_manager::instance().is_dark()
            ? D2D1::ColorF(0.12f, 0.12f, 0.14f, 1.0f)
            : D2D1::ColorF(0.98f, 0.98f, 1.0f, 1.0f);
        m_ui_renderer->clear(bg);

        if (m_toolbar) m_toolbar->render(*m_ui_renderer);

        // Content area: render either dashboard OR webview, never both
        bool dashboard_showing = m_toolbar && m_toolbar->is_dashboard_showing();
        if (dashboard_showing) {
            // Dashboard is rendered inside toolbar::render()
        } else if (m_tab_manager && m_tab_manager->active_tab()) {
            auto* active = m_tab_manager->active_tab();
            if (active->is_native()) {
                active->render_native(*m_ui_renderer, (float)m_toolbar->total_ui_height());
            }
        }

        m_ui_renderer->end_draw();
    }

    return 0;
}

} // namespace hyperion::browser
