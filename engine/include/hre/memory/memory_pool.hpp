#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <functional>

namespace hre::memory {

constexpr size_t DEFAULT_SLAB_SIZE = 4096;

class slab_allocator {
public:
    explicit slab_allocator(size_t block_size, size_t slab_size = DEFAULT_SLAB_SIZE);
    ~slab_allocator();

    void* allocate();
    void deallocate(void* ptr);

    size_t block_size() const { return m_block_size; }
    size_t slab_size() const { return m_slab_size; }
    size_t num_allocations() const { return m_num_allocations; }

    void reset();
    void release();

private:
    struct slab {
        uint8_t* memory = nullptr;
        size_t used = 0;
        slab* next = nullptr;
    };

    void grow_slab();

    size_t m_block_size;
    size_t m_slab_size;
    size_t m_num_allocations;

    slab* m_current_slab = nullptr;
    std::vector<std::unique_ptr<slab>> m_slabs;
};

class arena_allocator {
public:
    explicit arena_allocator(size_t initial_size = DEFAULT_SLAB_SIZE);
    ~arena_allocator();

    void* allocate(size_t size, size_t alignment = 8);
    void reset();
    void release();

    size_t used_memory() const { return m_used; }
    size_t total_allocated() const { return m_total; }

    template<typename T, typename... Args>
    T* create(Args&&... args);

    template<typename T>
    void destroy(T* ptr);

private:
    struct arena_block {
        uint8_t* memory = nullptr;
        size_t size = 0;
        size_t used = 0;
        arena_block* next = nullptr;
    };

    void grow(size_t min_size);

    size_t m_used = 0;
    size_t m_total = 0;
    arena_block* m_current = nullptr;
    std::vector<std::unique_ptr<arena_block>> m_blocks;
};

template<typename T, typename... Args>
T* arena_allocator::create(Args&&... args) {
    void* ptr = allocate(sizeof(T), alignof(T));
    return new (ptr) T(std::forward<Args>(args)...);
}

template<typename T>
void arena_allocator::destroy(T* ptr) {
    if (ptr) {
        ptr->~T();
    }
}

template<typename T>
class object_pool {
public:
    using constructor = std::function<T()>;

    explicit object_pool(size_t block_size = 128);
    ~object_pool();

    T* acquire();
    void release(T* obj);

    void clear();

    size_t num_objects() const { return m_num_objects; }
    size_t block_size() const { return m_block_size; }

private:
    struct slot {
        slot* next = nullptr;
        alignas(T) uint8_t data[sizeof(T)];
    };

    size_t m_block_size;
    size_t m_num_objects = 0;
    slot* m_free_list = nullptr;
    std::vector<std::unique_ptr<slab_allocator>> m_allocators;
};

template<typename T>
object_pool<T>::object_pool(size_t block_size)
    : m_block_size(block_size) {
    m_allocators.push_back(std::make_unique<slab_allocator>(sizeof(slot), block_size * sizeof(slot)));
}

template<typename T>
object_pool<T>::~object_pool() {
    clear();
}

template<typename T>
T* object_pool<T>::acquire() {
    slot* s = nullptr;

    if (m_free_list) {
        s = m_free_list;
        m_free_list = m_free_list->next;
    } else {
        if (m_allocators.empty() ||
            m_allocators.back()->num_allocations() >= m_allocators.back()->slab_size() / sizeof(slot)) {
            m_allocators.push_back(std::make_unique<slab_allocator>(sizeof(slot), m_block_size * sizeof(slot)));
        }
        s = static_cast<slot*>(m_allocators.back()->allocate());
    }

    ++m_num_objects;
    return new (s->data) T();
}

template<typename T>
void object_pool<T>::release(T* obj) {
    if (!obj) return;

    obj->~T();

    slot* s = reinterpret_cast<slot*>(obj);
    s->next = m_free_list;
    m_free_list = s;

    --m_num_objects;
}

template<typename T>
void object_pool<T>::clear() {
    m_free_list = nullptr;
    for (auto& alloc : m_allocators) {
        alloc->release();
    }
    m_allocators.clear();
    m_allocators.push_back(std::make_unique<slab_allocator>(sizeof(slot), m_block_size * sizeof(slot)));
    m_num_objects = 0;
}

} // namespace hre::memory