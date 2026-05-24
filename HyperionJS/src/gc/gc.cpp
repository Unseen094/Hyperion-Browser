#include <hjs/gc/gc.hpp>
#include <algorithm>

namespace hjs::gc {

GarbageCollector::GarbageCollector() = default;

GarbageCollector& GarbageCollector::instance() {
    static GarbageCollector gc;
    return gc;
}

void GarbageCollector::track_object(std::shared_ptr<JSObject> obj) {
    if (m_nursery.size() < NURSERY_THRESHOLD) {
        m_nursery.push_back(obj);
        m_nursery_allocated += sizeof(JSObject);
    } else {
        m_tracked.push_back(obj);
    }
}

void GarbageCollector::untrack_object(JSObject* obj) {
    auto it = m_roots.find(obj);
    if (it != m_roots.end()) m_roots.erase(it);
    auto nit = m_nursery_roots.find(obj);
    if (nit != m_nursery_roots.end()) m_nursery_roots.erase(nit);
}

void GarbageCollector::mark_root(const std::shared_ptr<JSObject>& root) {
    m_roots.insert(root.get());
}

void GarbageCollector::mark(JSObject* obj) {
    if (!obj) return;
    m_roots.insert(obj);
    m_nursery_roots.insert(obj);
}

void GarbageCollector::collect_nursery() {
    if (m_is_collecting) return;
    m_is_collecting = true;

    m_nursery_collections++;

    for (auto& root : m_roots) {
        (void)root;
        mark_nursery_roots();
    }

    sweep_nursery();

    m_is_collecting = false;
}

void GarbageCollector::mark_nursery_roots() {
    for (auto& wp : m_nursery) {
        if (auto obj = wp.lock()) {
            m_nursery_roots.insert(obj.get());
        }
    }
}

void GarbageCollector::promote_nursery(size_t idx) {
    if (idx < m_nursery.size()) {
        if (auto obj = m_nursery[idx].lock()) {
            m_tracked.push_back(obj);
            m_promotion_count++;
        }
        m_nursery[idx].reset();
    }
}

void GarbageCollector::sweep_nursery() {
    std::vector<std::weak_ptr<JSObject>> survivors;

    for (auto& wp : m_nursery) {
        if (auto obj = wp.lock()) {
            if (m_nursery_roots.find(obj.get()) != m_nursery_roots.end()) {
                survivors.push_back(obj);
            } else {
                m_total_collected++;
            }
        }
    }

    m_nursery = std::move(survivors);

    if (m_nursery_collections % 5 == 0 && !m_nursery.empty()) {
        for (auto& wp : m_nursery) {
            if (auto obj = wp.lock()) {
                m_tracked.push_back(obj);
            }
        }
        m_nursery.clear();
    }

    m_nursery_roots.clear();
}

void GarbageCollector::collect() {
    if (m_is_collecting) return;
    m_is_collecting = true;

    // Step 1: Collect nursery first
    collect_nursery();

    // Step 2: Mark phase
    std::set<JSObject*> marked;
    for (auto& root_ptr : m_roots) {
        marked.insert(root_ptr);
    }

    for (auto& wp : m_tracked) {
        if (auto obj = wp.lock()) {
            if (marked.find(obj.get()) != marked.end()) {
                continue;
            }
            m_total_collected++;
        }
    }

    m_tracked.erase(
        std::remove_if(m_tracked.begin(), m_tracked.end(),
            [&marked](const std::weak_ptr<JSObject>& wp) {
                if (auto obj = wp.lock()) {
                    return marked.find(obj.get()) == marked.end();
                }
                return true;
            }),
        m_tracked.end()
    );

    m_is_collecting = false;
}

void GarbageCollector::sweep() {
    // Sweep is now handled inside collect()
}

void GarbageCollector::write_barrier(JSObject* obj) {
    if (m_nursery_roots.find(obj) != m_nursery_roots.end()) {
        return;
    }
    for (auto& wp : m_nursery) {
        if (auto nobj = wp.lock()) {
            if (nobj.get() == obj) {
                promote_nursery(0);
                break;
            }
        }
    }
}

} // namespace hjs::gc
