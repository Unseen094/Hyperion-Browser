#pragma once

#include <hyperion/ui/renderer.hpp>
#include <hyperion/ui/theme_manager.hpp>
#include <string>
#include <functional>
#include <vector>

namespace hyperion::ui {

class accessibility_handler {
public:
    static accessibility_handler& instance() {
        static accessibility_handler inst;
        return inst;
    }

    accessibility_handler();
    ~accessibility_handler();

    void initialize(HWND hwnd);
    bool is_high_contrast() const;
    bool is_narrator_active() const;
    void announce(const std::wstring& text);
    void set_accessible_name(HWND hwnd, const std::wstring& name);
    void set_accessible_role(HWND hwnd, const std::wstring& role);

    void on_high_contrast_changed(std::function<void()> cb) { m_hc_callbacks.push_back(std::move(cb)); }

private:
    void detect_high_contrast();
    HWND m_hwnd = nullptr;
    std::vector<std::function<void()>> m_hc_callbacks;
    bool m_high_contrast = false;
    bool m_initialized = false;
};

class title_ai {
public:
    static title_ai& instance() {
        static title_ai inst;
        return inst;
    }

    title_ai() = default;
    ~title_ai() = default;

    void suggest_title(const std::wstring& url,
                       std::function<void(const std::wstring&)> callback);

private:
    std::wstring extract_domain(const std::wstring& url) const;
    std::wstring capitalize(const std::wstring& s) const;
};

} // namespace hyperion::ui
