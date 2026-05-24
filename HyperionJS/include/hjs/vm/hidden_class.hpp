#pragma once

#include <hjs/runtime/object.hpp>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <memory>

namespace hjs::vm {

struct PropertyTransition {
    std::wstring name;
    size_t next_shape_index;
};

class Shape {
public:
    Shape() = default;

    size_t add_property(const std::wstring& name);
    int lookup_property(const std::wstring& name) const;
    size_t shape_index() const { return m_shape_index; }
    void set_shape_index(size_t idx) { m_shape_index = idx; }

    const std::vector<std::wstring>& property_chain() const { return m_property_chain; }
    const std::vector<PropertyTransition>& transitions() const { return m_transitions; }
    size_t object_count() const { return m_object_count; }
    void increment_object_count() { m_object_count++; }
    void decrement_object_count() { if (m_object_count > 0) m_object_count--; }

    bool has_transition(const std::wstring& name) const;
    size_t transition_to(const std::wstring& name);
    void add_transition(const std::wstring& name, size_t next_index) { m_transitions.push_back({name, next_index}); }

private:
    size_t m_shape_index = 0;
    std::vector<std::wstring> m_property_chain;
    std::vector<PropertyTransition> m_transitions;
    std::unordered_map<std::wstring, int> m_property_offset;
    size_t m_object_count = 0;
};

class ShapeRoot {
public:
    static ShapeRoot& instance();

    std::shared_ptr<Shape> empty_shape();
    std::shared_ptr<Shape> get_or_create_transition(std::shared_ptr<Shape> current, const std::wstring& name);
    void track_object_shape(JSObject* obj);
    void untrack_object_shape(JSObject* obj);

private:
    ShapeRoot();
    std::vector<std::shared_ptr<Shape>> m_shapes;
    std::shared_ptr<Shape> m_empty;

    // Shape map: shape_index -> set of object pointers
    std::unordered_map<size_t, std::unordered_set<JSObject*>> m_shape_map;
    std::unordered_map<JSObject*, size_t> m_object_shape;
};

struct ShapeGuard {
    std::shared_ptr<Shape> shape;
    size_t expected_generation;
};

} // namespace hjs::vm
