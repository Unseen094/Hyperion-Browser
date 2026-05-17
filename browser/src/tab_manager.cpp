#include <hyperion/browser/tab_manager.hpp>
#include <hyperion/platform/logging.hpp>

namespace hyperion::browser {

tab_manager::tab_manager() {}
tab_manager::~tab_manager() {}

tab* tab_manager::create_tab(const std::wstring& url) {
    auto new_tab = std::make_unique<tab>(url);
    tab* ptr = new_tab.get();
    
    // Hide current active tab
    if (m_active_index < m_tabs.size()) {
        m_tabs[m_active_index]->set_visible(false);
    }

    m_tabs.push_back(std::move(new_tab));
    m_active_index = m_tabs.size() - 1;
    
    // Show new tab
    m_tabs[m_active_index]->set_visible(true);
    
    return ptr;
}

void tab_manager::close_tab(size_t index) {
    if (index < m_tabs.size()) {
        m_tabs.erase(m_tabs.begin() + index);
        if (m_active_index >= m_tabs.size() && !m_tabs.empty()) {
            m_active_index = m_tabs.size() - 1;
            m_tabs[m_active_index]->set_visible(true);
        }
    }
}

void tab_manager::set_active_tab(size_t index) {
    if (index < m_tabs.size() && index != m_active_index) {
        if (m_active_index < m_tabs.size()) {
            m_tabs[m_active_index]->set_visible(false);
        }
        m_active_index = index;
        m_tabs[m_active_index]->set_visible(true);
    }
}

tab* tab_manager::active_tab() const {
    if (m_active_index < m_tabs.size()) {
        return m_tabs[m_active_index].get();
    }
    return nullptr;
}

} // namespace hyperion::browser
