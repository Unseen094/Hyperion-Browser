#pragma once

#include <string>
#include <vector>

namespace hre::sandbox {

// Extended sandbox policy interface for Phase 26 — Sandbox
// Implements Win32 AppContainer, process mitigations, and capability restriction.

class sandbox {
public:
    // AppContainer
    bool create_appcontainer(const std::wstring& name, const std::vector<std::wstring>& capabilities);

    // Process mitigations (CIG, ACG, CFG, SEHOP, DEP)
    bool apply_process_mitigations();

    // File/registry capability restriction
    bool restrict_filesystem(const std::vector<std::wstring>& allowed_paths,
                             const std::vector<std::wstring>& blocked_paths);
    bool restrict_registry(const std::vector<std::wstring>& allowed_keys,
                           const std::vector<std::wstring>& blocked_keys);

    // Integrity level
    bool setup_low_integrity_level();

    // Network restriction via WFP
    bool restrict_network();

    // Resource limits (memory, CPU)
    bool apply_resource_limits();

private:
    std::wstring m_appcontainer_name;
    std::vector<std::wstring> m_appcontainer_caps;
    std::vector<std::wstring> m_allowed_paths;
    std::vector<std::wstring> m_blocked_paths;
    std::vector<std::wstring> m_allowed_registry;
    std::vector<std::wstring> m_blocked_registry;
};

} // namespace hre::sandbox
