#pragma once

#include <hjs/core/value.hpp>
#include <hjs/runtime/object.hpp>
#include <vector>
#include <memory>
#include <set>
#include <cstdint>

namespace hjs::gc {

class GarbageCollector {
public:
    GarbageCollector();

    void track_object(std::shared_ptr<JSObject> obj);
    void untrack_object(JSObject* obj);

    void collect();
    void collect_nursery();
    size_t allocation_count() const { return m_tracked.size() + m_nursery.size(); }
    size_t collected_count() const { return m_total_collected; }

    void set_threshold(size_t t) { m_threshold = t; }
    bool should_collect() const { return m_tracked.size() + m_nursery.size() >= m_threshold; }

    void mark_root(const std::shared_ptr<JSObject>& root);

    static GarbageCollector& instance();

private:
    void mark(JSObject* obj);
    void sweep();

    void mark_nursery_roots();
    void promote_nursery(size_t idx);
    void sweep_nursery();

    void write_barrier(JSObject* obj);

    // Tenured generation (old space)
    std::vector<std::weak_ptr<JSObject>> m_tracked;
    std::set<JSObject*> m_roots;
    size_t m_threshold = 1000;
    size_t m_total_collected = 0;
    bool m_is_collecting = false;

    // Nursery generation (young space)
    static constexpr size_t NURSERY_SIZE = 1024 * 1024; // 1MB
    static constexpr size_t NURSERY_THRESHOLD = 128;
    std::vector<std::weak_ptr<JSObject>> m_nursery;
    std::set<JSObject*> m_nursery_roots;
    size_t m_nursery_allocated = 0;
    size_t m_nursery_collections = 0;
    int m_promotion_count = 0;
};

} // namespace hjs::gc
