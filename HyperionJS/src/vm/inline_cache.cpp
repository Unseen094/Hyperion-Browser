#include <hjs/vm/inline_cache.hpp>

namespace hjs::vm {

InlineCache::InlineCache()
    : m_state(ICState::Uninitialized)
    , m_entry_count(0)
    , m_hits(0)
    , m_misses(0)
    , m_total_lookups(0)
{
    for (auto& e : m_entries) e.valid = false;
}

bool InlineCache::lookup(const JSObject* obj, const std::wstring& property_name, Value& out_result, bool& out_is_method) {
    m_total_lookups++;

    if (m_state == ICState::Uninitialized) {
        m_misses++;
        return false;
    }

    size_t obj_gen = obj->shape_generation();

    for (int i = 0; i < m_entry_count; i++) {
        if (!m_entries[i].valid) continue;
        if (m_entries[i].shape_generation == obj_gen) {
            if (m_entries[i].shape && m_entries[i].shape->lookup_property(property_name) >= 0) {
                m_hits++;
                out_result = m_entries[i].cached_value;
                out_is_method = m_entries[i].is_method;
                return true;
            }
        }
    }

    m_misses++;
    if (m_state == ICState::Monomorphic && m_entry_count == 1) {
        m_state = ICState::Polymorphic;
    } else if (m_state == ICState::Polymorphic && m_entry_count >= IC_MAX_SHAPES) {
        m_state = ICState::Megamorphic;
    }
    return false;
}

void InlineCache::update(const JSObject* obj, const std::wstring& property_name, const Value& value, bool is_method) {
    if (m_state == ICState::Megamorphic) return;

    size_t obj_gen = obj->shape_generation();

    for (int i = 0; i < m_entry_count; i++) {
        if (m_entries[i].shape_generation == obj_gen) {
            m_entries[i].cached_value = value;
            m_entries[i].is_method = is_method;
            m_entries[i].valid = true;
            return;
        }
    }

    if (m_entry_count < IC_MAX_SHAPES) {
        auto shape = ShapeRoot::instance().get_or_create_transition(
            ShapeRoot::instance().empty_shape(), property_name);
        m_entries[m_entry_count].shape = shape;
        m_entries[m_entry_count].shape_generation = obj_gen;
        m_entries[m_entry_count].cached_value = value;
        m_entries[m_entry_count].is_method = is_method;
        m_entries[m_entry_count].valid = true;
        m_entry_count++;

        if (m_entry_count == 1) m_state = ICState::Monomorphic;
    }
}

void InlineCache::invalidate() {
    m_state = ICState::Uninitialized;
    m_entry_count = 0;
    for (auto& e : m_entries) e.valid = false;
}

} // namespace hjs::vm
