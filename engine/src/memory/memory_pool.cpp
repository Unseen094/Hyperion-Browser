#include <hre/memory/memory_pool.hpp>
#include <cstdlib>
#include <cstring>

namespace hre::memory {

slab_allocator::slab_allocator(size_t block_size, size_t slab_size)
    : m_block_size(block_size)
    , m_slab_size(slab_size)
    , m_num_allocations(0) {
    if (m_block_size < sizeof(void*)) {
        m_block_size = sizeof(void*);
    }
    size_t align = m_block_size;
    m_block_size = ((m_block_size + align - 1) / align) * align;
    grow_slab();
}

slab_allocator::~slab_allocator() {
    release();
}

void* slab_allocator::allocate() {
    if (!m_current_slab || m_current_slab->used + m_block_size > m_slab_size) {
        grow_slab();
    }

    void* ptr = m_current_slab->memory + m_current_slab->used;
    m_current_slab->used += m_block_size;
    ++m_num_allocations;

    return ptr;
}

void slab_allocator::deallocate(void* ptr) {
}

void slab_allocator::reset() {
    for (auto& slab : m_slabs) {
        slab->used = 0;
    }
    m_current_slab = m_slabs.empty() ? nullptr : m_slabs[0].get();
    m_num_allocations = 0;
}

void slab_allocator::release() {
    m_slabs.clear();
    m_current_slab = nullptr;
    m_num_allocations = 0;
}

void slab_allocator::grow_slab() {
    auto slab_ptr = std::make_unique<slab>();
    slab_ptr->memory = static_cast<uint8_t*>(std::malloc(m_slab_size));
    slab_ptr->used = 0;
    slab_ptr->next = nullptr;

    if (!m_slabs.empty()) {
        m_slabs.back()->next = slab_ptr.get();
    }
    m_slabs.push_back(std::move(slab_ptr));
    m_current_slab = m_slabs.back().get();
}

arena_allocator::arena_allocator(size_t initial_size) {
    grow(initial_size);
}

arena_allocator::~arena_allocator() {
    release();
}

void* arena_allocator::allocate(size_t size, size_t alignment) {
    size = (size + alignment - 1) & ~(alignment - 1);

    if (!m_current || m_current->used + size > m_current->size) {
        grow(size);
    }

    void* ptr = m_current->memory + m_current->used;
    m_current->used += size;
    m_used += size;

    std::memset(ptr, 0, size);
    return ptr;
}

void arena_allocator::reset() {
    for (auto& block : m_blocks) {
        block->used = 0;
    }
    m_current = m_blocks.empty() ? nullptr : m_blocks[0].get();
    m_used = 0;
}

void arena_allocator::release() {
    m_blocks.clear();
    m_current = nullptr;
    m_used = 0;
    m_total = 0;
}

void arena_allocator::grow(size_t min_size) {
    size_t size = std::max(min_size, DEFAULT_SLAB_SIZE);

    auto block = std::make_unique<arena_block>();
    block->memory = static_cast<uint8_t*>(std::malloc(size));
    block->size = size;
    block->used = 0;
    block->next = nullptr;

    if (!m_blocks.empty()) {
        m_blocks.back()->next = block.get();
    }
    m_blocks.push_back(std::move(block));
    m_current = m_blocks.back().get();
    m_total += m_current->size;
}

} // namespace hre::memory