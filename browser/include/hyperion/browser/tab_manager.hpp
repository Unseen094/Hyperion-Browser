#pragma once

#include <hyperion/browser/tab.hpp>
#include <vector>
#include <memory>
#include <optional>

namespace hyperion::browser {

class tab_manager {
public:
    tab_manager();
    ~tab_manager();

    tab* create_tab(const std::wstring& url = L"about:blank");
    void close_tab(size_t index);
    
    void set_active_tab(size_t index);
    tab* active_tab() const;

    const std::vector<std::unique_ptr<tab>>& tabs() const { return m_tabs; }
    size_t active_index() const { return m_active_index; }

private:
    std::vector<std::unique_ptr<tab>> m_tabs;
    size_t m_active_index = 0;
};

} // namespace hyperion::browser
