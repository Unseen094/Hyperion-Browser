#pragma once

#include <hjs/core/value.hpp>
#include <hjs/runtime/object.hpp>
#include <hjs/vm/hidden_class.hpp>
#include <array>
#include <cstdint>

namespace hjs::vm {

static constexpr int IC_MAX_SHAPES = 16;

enum class ICState : uint8_t {
    Uninitialized,
    Monomorphic,
    Polymorphic,
    Megamorphic
};

struct ICEntry {
    std::shared_ptr<Shape> shape;
    size_t shape_generation = 0;
    Value cached_value;
    bool is_method = false;
    bool valid = false;
};

class InlineCache {
public:
    InlineCache();

    bool lookup(const JSObject* obj, const std::wstring& property_name, Value& out_result, bool& out_is_method);
    void update(const JSObject* obj, const std::wstring& property_name, const Value& value, bool is_method);
    void invalidate();

    ICState state() const { return m_state; }
    int entry_count() const { return m_entry_count; }
    int hit_count() const { return m_hits; }
    int miss_count() const { return m_misses; }

private:
    ICState m_state;
    std::array<ICEntry, IC_MAX_SHAPES> m_entries;
    int m_entry_count;
    int m_hits;
    int m_misses;
    int m_total_lookups;
};

} // namespace hjs::vm
