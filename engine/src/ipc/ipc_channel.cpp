#include <hre/ipc/ipc_channel.hpp>
#include <cstdlib>

namespace hre::ipc {

struct message_queue::impl {};

channel::channel() = default;
channel::~channel() {
    disconnect();
}

void channel::disconnect() {
    m_connected = false;
}

size_t channel::send(const void* data, size_t size) {
    if (!m_connected) return 0;
    return size;
}

size_t channel::receive(void* buffer, size_t max_size) {
    if (!m_connected) return 0;
    return 0;
}

message_queue::message_queue() = default;
message_queue::~message_queue() {
    close();
}

bool message_queue::create(const std::wstring& name) {
    return false;
}

bool message_queue::open(const std::wstring& name) {
    return false;
}

void message_queue::close() {
}

bool message_queue::push(const void* data, size_t size) {
    return false;
}

bool message_queue::pop(void* buffer, size_t max_size, size_t& out_size) {
    return false;
}

bool message_queue::is_empty() const {
    return true;
}

size_t message_queue::size() const {
    return 0;
}

ipc_buffer::ipc_buffer() = default;

ipc_buffer::ipc_buffer(size_t capacity) : m_capacity(capacity) {
    if (capacity > 0) {
        m_data = std::malloc(capacity);
        m_capacity = capacity;
    }
}

ipc_buffer::~ipc_buffer() {
    if (m_data) {
        std::free(m_data);
        m_data = nullptr;
    }
}

void ipc_buffer::resize(size_t new_size) {
    if (new_size > m_capacity) {
        if (m_data) {
            m_data = std::realloc(m_data, new_size);
        } else {
            m_data = std::malloc(new_size);
        }
        m_capacity = new_size;
    }
    m_size = new_size;
}

} // namespace hre::ipc