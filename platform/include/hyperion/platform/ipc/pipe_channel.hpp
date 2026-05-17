#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace hyperion::platform::ipc {

enum class message_type : uint32_t {
    NONE = 0,
    NAVIGATE,           // Browser -> Renderer: URL to load
    RENDER_FRAME,       // Renderer -> Browser: Bitmap/Command list
    INPUT_EVENT,        // Browser -> Renderer: Mouse/Keyboard
    DOM_READY,          // Renderer -> Browser: Document parsed
    SCRIPT_LOG,         // Renderer -> Browser: console.log
    REQUEST_RESOURCE,   // Renderer -> Browser: Fetch URL
    RESOURCE_DATA       // Browser -> Renderer: File data
};

struct message_header {
    message_type type;
    uint32_t payload_size;
};

struct message {
    message_header header;
    std::vector<uint8_t> payload;
};

class pipe_channel {
public:
    pipe_channel();
    ~pipe_channel();

    // Server side (Browser)
    bool create(const std::wstring& name);
    bool listen();

    // Client side (Renderer)
    bool connect(const std::wstring& name);

    // Common
    bool send(const message& msg);
    bool receive(message& msg);
    void close();

    bool is_connected() const { return m_connected; }

private:
    void* m_pipe_handle = (void*)-1; // INVALID_HANDLE_VALUE
    bool m_connected = false;
    bool m_is_server = false;
};

} // namespace hyperion::platform::ipc
