#include "../include/hyperion/platform/ipc/pipe_channel.hpp"
#include <windows.h>
#include <string>
#include <vector>

namespace hyperion::platform::ipc {

pipe_channel::pipe_channel() 
    : m_pipe_handle(INVALID_HANDLE_VALUE), m_connected(false), m_is_server(false) {}

pipe_channel::~pipe_channel() {
    close();
}

bool pipe_channel::create(const std::wstring& name) {
    std::wstring full_pipe_name = L"\\\\.\\pipe\\" + name;
    
    m_pipe_handle = CreateNamedPipeW(
        full_pipe_name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        1024 * 1024, // 1MB Output Buffer for larger frames
        1024 * 1024, // 1MB Input Buffer
        0,           // Default client timeout
        nullptr      // Default security attributes
    );

    if (m_pipe_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    m_is_server = true;
    m_connected = false;
    return true;
}

bool pipe_channel::listen() {
    if (m_pipe_handle == INVALID_HANDLE_VALUE || !m_is_server) {
        return false;
    }

    // ConnectNamedPipe will block until a client connects or returns TRUE/FALSE with ERROR_PIPE_CONNECTED
    BOOL result = ConnectNamedPipe(m_pipe_handle, nullptr);
    if (result) {
        m_connected = true;
        return true;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_PIPE_CONNECTED) {
            m_connected = true;
            return true;
        }
    }
    return false;
}

bool pipe_channel::connect(const std::wstring& name) {
    std::wstring full_pipe_name = L"\\\\.\\pipe\\" + name;

    // Retry loop to connect if the pipe is busy
    while (true) {
        m_pipe_handle = CreateFileW(
            full_pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,             // No sharing
            nullptr,       // Default security
            OPEN_EXISTING, // Opens existing pipe
            0,             // Default attributes
            nullptr        // No template file
        );

        if (m_pipe_handle != INVALID_HANDLE_VALUE) {
            break;
        }

        if (GetLastError() != ERROR_PIPE_BUSY) {
            return false;
        }

        // Wait up to 20 seconds for the pipe to become available
        if (!WaitNamedPipeW(full_pipe_name.c_str(), 20000)) {
            return false;
        }
    }

    // Set message read mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(m_pipe_handle, &mode, nullptr, nullptr)) {
        CloseHandle(m_pipe_handle);
        m_pipe_handle = INVALID_HANDLE_VALUE;
        return false;
    }

    m_connected = true;
    m_is_server = false;
    return true;
}

bool pipe_channel::send(const message& msg) {
    if (m_pipe_handle == INVALID_HANDLE_VALUE || !m_connected) {
        return false;
    }

    DWORD written = 0;
    
    // 1. Write message header
    if (!WriteFile(m_pipe_handle, &msg.header, sizeof(msg.header), &written, nullptr)) {
        return false;
    }

    if (written != sizeof(msg.header)) {
        return false;
    }

    // 2. Write payload if present
    if (msg.header.payload_size > 0 && !msg.payload.empty()) {
        if (!WriteFile(m_pipe_handle, msg.payload.data(), msg.header.payload_size, &written, nullptr)) {
            return false;
        }
        if (written != msg.header.payload_size) {
            return false;
        }
    }

    return true;
}

bool pipe_channel::receive(message& msg) {
    if (m_pipe_handle == INVALID_HANDLE_VALUE || !m_connected) {
        return false;
    }

    DWORD read = 0;
    
    // 1. Read message header
    if (!ReadFile(m_pipe_handle, &msg.header, sizeof(msg.header), &read, nullptr)) {
        return false;
    }

    if (read != sizeof(msg.header)) {
        return false;
    }

    // 2. Read payload if size > 0
    if (msg.header.payload_size > 0) {
        msg.payload.resize(msg.header.payload_size);
        if (!ReadFile(m_pipe_handle, msg.payload.data(), msg.header.payload_size, &read, nullptr)) {
            return false;
        }
        if (read != msg.header.payload_size) {
            return false;
        }
    } else {
        msg.payload.clear();
    }

    return true;
}

void pipe_channel::close() {
    if (m_pipe_handle != INVALID_HANDLE_VALUE) {
        if (m_connected && m_is_server) {
            // Flush buffers
            FlushFileBuffers(m_pipe_handle);
            DisconnectNamedPipe(m_pipe_handle);
        }
        CloseHandle(m_pipe_handle);
        m_pipe_handle = INVALID_HANDLE_VALUE;
    }
    m_connected = false;
    m_is_server = false;
}

} // namespace hyperion::platform::ipc
