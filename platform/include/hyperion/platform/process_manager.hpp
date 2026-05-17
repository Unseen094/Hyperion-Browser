#pragma once

#include <string>
#include <map>
#include <vector>
#include <windows.h>

namespace hyperion::platform {

struct renderer_process {
    DWORD process_id;
    HANDLE job_handle;
    HANDLE process_handle;
};

class process_manager {
public:
    process_manager();
    ~process_manager();

    // Spawns a new renderer process with IPC pipe and sandboxing
    bool spawn_renderer(const std::string& pipe_name);

    // Terminate a specific renderer process
    bool terminate_renderer(DWORD process_id);

    // Get all tracked renderer process IDs
    std::vector<DWORD> get_renderer_ids();

private:
    std::map<DWORD, renderer_process> m_renderers;
};

} // namespace hyperion::platform
