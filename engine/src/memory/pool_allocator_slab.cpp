#include <hre/memory/memory_pool.hpp>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <string>

namespace hre::memory {

// ---- Slab allocator (power-of-2 buckets) ----

class slab_bucket {
public:
    slab_bucket(size_t block_size, size_t slab_size = 65536)
        : block_size_(block_size), slab_size_(slab_size) {}

    void* allocate() {
        if (free_list_) {
            auto* p = free_list_;
            free_list_ = *reinterpret_cast<void**>(free_list_);
            return p;
        }
        if (!current_ || current_used_ + block_size_ > slab_size_) {
            grow();
        }
        void* p = current_ + current_used_;
        current_used_ += block_size_;
        return p;
    }

    void deallocate(void* p) {
        *reinterpret_cast<void**>(p) = free_list_;
        free_list_ = p;
    }

    void reset() {
        free_list_ = nullptr;
        current_used_ = 0;
        current_ = slabs_.empty() ? nullptr : slabs_[0].get();
    }

    void release() {
        slabs_.clear();
        free_list_ = nullptr;
        current_used_ = 0;
        current_ = nullptr;
    }

    size_t block_size() const { return block_size_; }

private:
    void grow() {
        auto slab = std::make_unique<uint8_t[]>(slab_size_);
        current_ = slab.get();
        slabs_.push_back(std::move(slab));
        current_used_ = 0;
    }

    size_t block_size_;
    size_t slab_size_;
    uint8_t* current_ = nullptr;
    size_t current_used_ = 0;
    void* free_list_ = nullptr;
    std::vector<std::unique_ptr<uint8_t[]>> slabs_;
};

static std::unordered_map<size_t, std::unique_ptr<slab_bucket>> g_slab_buckets;

// ---- Fixed-size power-of-2 slab ----

void* slab_allocate(size_t size) {
    size_t block_size = 8;
    while (block_size < size) block_size *= 2;

    auto it = g_slab_buckets.find(block_size);
    if (it == g_slab_buckets.end()) {
        auto bucket = std::make_unique<slab_bucket>(block_size);
        it = g_slab_buckets.emplace(block_size, std::move(bucket)).first;
    }
    return it->second->allocate();
}

void slab_deallocate(void* p, size_t size) {
    size_t block_size = 8;
    while (block_size < size) block_size *= 2;

    auto it = g_slab_buckets.find(block_size);
    if (it != g_slab_buckets.end()) {
        it->second->deallocate(p);
    }
}

// ---- TLSF (Two-Level Segregated Fit) ----
// Simplified implementation for variable-size allocation

struct tlsf_segment {
    tlsf_segment* next;
    size_t size;
    bool free;
};

static tlsf_segment* g_tlsf_head = nullptr;
static constexpr size_t TLSF_MIN_BLOCK = 16;
static constexpr size_t TLSF_ALIGN = 16;

void tlsf_init(size_t heap_size) {
    void* mem = std::malloc(heap_size);
    if (!mem) return;

    g_tlsf_head = static_cast<tlsf_segment*>(mem);
    g_tlsf_head->next = nullptr;
    g_tlsf_head->size = heap_size;
    g_tlsf_head->free = true;
}

void* tlsf_allocate(size_t size) {
    size = (size + TLSF_ALIGN - 1) & ~(TLSF_ALIGN - 1);
    if (size < TLSF_MIN_BLOCK) size = TLSF_MIN_BLOCK;

    tlsf_segment* best = nullptr;
    tlsf_segment* best_prev = nullptr;
    tlsf_segment* prev = nullptr;
    tlsf_segment* curr = g_tlsf_head;

    while (curr) {
        if (curr->free && curr->size >= size) {
            if (!best || curr->size < best->size) {
                best = curr;
                best_prev = prev;
                if (curr->size == size) break;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    if (!best) return nullptr;

    size_t remaining = best->size - size;
    if (remaining >= TLSF_MIN_BLOCK + sizeof(tlsf_segment)) {
        auto* new_seg = reinterpret_cast<tlsf_segment*>(
            reinterpret_cast<uint8_t*>(best) + sizeof(tlsf_segment) + size);
        new_seg->size = remaining - sizeof(tlsf_segment);
        new_seg->free = true;
        new_seg->next = best->next;
        best->next = new_seg;
        best->size = size;
    }

    best->free = false;
    return reinterpret_cast<uint8_t*>(best) + sizeof(tlsf_segment);
}

void tlsf_deallocate(void* p) {
    if (!p) return;
    auto* seg = reinterpret_cast<tlsf_segment*>(
        static_cast<uint8_t*>(p) - sizeof(tlsf_segment));
    seg->free = true;

    // Coalesce with next
    if (seg->next && seg->next->free) {
        seg->size += sizeof(tlsf_segment) + seg->next->size;
        seg->next = seg->next->next;
    }

    // Coalesce with prev
    tlsf_segment* prev = nullptr;
    tlsf_segment* curr = g_tlsf_head;
    while (curr && curr != seg) {
        prev = curr;
        curr = curr->next;
    }
    if (prev && prev->free) {
        prev->size += sizeof(tlsf_segment) + seg->size;
        prev->next = seg->next;
    }
}

// ---- Per-subsystem arena allocation ----

static std::unordered_map<std::string, std::unique_ptr<arena_allocator>> g_subsystem_arenas;

arena_allocator* get_subsystem_arena(const std::string& name) {
    auto it = g_subsystem_arenas.find(name);
    if (it != g_subsystem_arenas.end()) {
        return it->second.get();
    }
    auto arena = std::make_unique<arena_allocator>(65536);
    auto* ptr = arena.get();
    g_subsystem_arenas[name] = std::move(arena);
    return ptr;
}

void release_subsystem_arena(const std::string& name) {
    g_subsystem_arenas.erase(name);
}

// ---- Memory pressure callback ----

static std::function<void(size_t)> g_pressure_callback;

void set_memory_pressure_callback(std::function<void(size_t)> cb) {
    g_pressure_callback = std::move(cb);
}

void notify_memory_pressure(size_t current_usage) {
    if (g_pressure_callback) {
        g_pressure_callback(current_usage);
    }
}

} // namespace hre::memory
