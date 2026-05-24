#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace hre::ipc {

enum class channel_type {
    ANONYMOUS,
    NAMED_PIPE,
    SHARED_MEMORY
};

struct message_header {
    uint32_t magic;
    uint32_t type;
    uint32_t size;
    uint64_t id;
    uint64_t timestamp;
};

class channel {
public:
    channel();
    ~channel();

    bool is_connected() const { return m_connected; }
    void disconnect();

    size_t send(const void* data, size_t size);
    size_t receive(void* buffer, size_t max_size);

    using message_callback = std::function<void(const void*, size_t)>;
    void set_message_callback(message_callback cb) { m_message_callback = cb; }

    uint64_t peer_id() const { return m_peer_id; }
    void set_peer_id(uint64_t id) { m_peer_id = id; }

private:
    bool m_connected = false;
    uint64_t m_peer_id = 0;
    message_callback m_message_callback;
};

class message_queue {
public:
    message_queue();
    ~message_queue();

    bool create(const std::wstring& name);
    bool open(const std::wstring& name);

    void close();

    bool push(const void* data, size_t size);
    bool pop(void* buffer, size_t max_size, size_t& out_size);
    bool is_empty() const;
    size_t size() const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

class ipc_buffer {
public:
    ipc_buffer();
    explicit ipc_buffer(size_t capacity);
    ~ipc_buffer();

    void* data() { return m_data; }
    const void* data() const { return m_data; }
    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }

    void resize(size_t new_size);

    template<typename T>
    T* as() { return reinterpret_cast<T*>(m_data); }

    template<typename T>
    const T* as() const { return reinterpret_cast<const T*>(m_data); }

private:
    void* m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
};

} // namespace hre::ipc