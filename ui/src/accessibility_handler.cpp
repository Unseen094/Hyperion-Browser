#include <hyperion/ui/accessibility_handler.hpp>
#include <hyperion/platform/logging.hpp>
#include <windows.h>

#ifndef SPI_GETNARRATOR
#define SPI_GETNARRATOR 0x0046
#endif

namespace hyperion::ui {

accessibility_handler::accessibility_handler() = default;
accessibility_handler::~accessibility_handler() = default;

void accessibility_handler::initialize(HWND hwnd) {
    m_hwnd = hwnd;
    detect_high_contrast();
    m_initialized = true;
    HYPERION_LOG_INFO("Accessibility handler initialized, HC: {}", m_high_contrast ? "true" : "false");
}

bool accessibility_handler::is_high_contrast() const {
    return m_high_contrast;
}

bool accessibility_handler::is_narrator_active() const {
    BOOL narrated = FALSE;
    SystemParametersInfoW(SPI_GETNARRATOR, 0, &narrated, 0);
    return narrated != FALSE;
}

void accessibility_handler::announce(const std::wstring& text) {
    if (!m_hwnd) return;
    HYPERION_LOG_INFO("A11Y announce called");
    // In a full implementation, this would use UIA NotifyWinEvent
    NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, m_hwnd, OBJID_CLIENT, CHILDID_SELF);
}

void accessibility_handler::set_accessible_name(HWND hwnd, const std::wstring& name) {
    // Would set the accessible name property via UIA
    (void)hwnd; (void)name;
}

void accessibility_handler::set_accessible_role(HWND hwnd, const std::wstring& role) {
    (void)hwnd; (void)role;
}

void accessibility_handler::detect_high_contrast() {
    HIGHCONTRASTW hc = {};
    hc.cbSize = sizeof(HIGHCONTRASTW);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRASTW), &hc, 0)) {
        m_high_contrast = (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
        if (m_high_contrast) {
            HYPERION_LOG_INFO("High contrast mode detected");
            theme_manager::instance().set_theme(THEME_HIGH_CONTRAST);
            for (auto& cb : m_hc_callbacks) {
                if (cb) cb();
            }
        }
    }
}

// === title_ai ===

void title_ai::suggest_title(const std::wstring& url,
                              std::function<void(const std::wstring&)> callback) {
    std::wstring domain = extract_domain(url);
    if (domain.empty()) {
        if (callback) callback(L"New Tab");
        return;
    }
    if (callback) callback(capitalize(domain));
}

std::wstring title_ai::extract_domain(const std::wstring& url) const {
    if (url.empty()) return L"";
    size_t start = url.find(L"://");
    if (start == std::wstring::npos) start = 0;
    else start += 3;
    size_t end = url.find(L'/', start);
    if (end == std::wstring::npos) end = url.length();
    std::wstring domain = url.substr(start, end - start);
    size_t dot = domain.find(L'.');
    if (dot != std::wstring::npos) {
        size_t prev_dot = domain.rfind(L'.', dot - 1);
        if (prev_dot != std::wstring::npos) {
            return domain.substr(prev_dot + 1, dot - prev_dot - 1);
        }
        return domain.substr(0, dot);
    }
    return domain;
}

std::wstring title_ai::capitalize(const std::wstring& s) const {
    if (s.empty()) return s;
    std::wstring result = s;
    result[0] = towupper(result[0]);
    return result;
}

} // namespace hyperion::ui
