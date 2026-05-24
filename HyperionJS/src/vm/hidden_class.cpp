#include <hjs/vm/hidden_class.hpp>
#include <algorithm>

namespace hjs::vm {

size_t Shape::add_property(const std::wstring& name) {
    if (m_property_offset.count(name)) {
        return m_property_offset[name];
    }
    int offset = (int)m_property_chain.size();
    m_property_chain.push_back(name);
    m_property_offset[name] = offset;
    return offset;
}

int Shape::lookup_property(const std::wstring& name) const {
    auto it = m_property_offset.find(name);
    if (it != m_property_offset.end()) return it->second;
    return -1;
}

bool Shape::has_transition(const std::wstring& name) const {
    for (const auto& t : m_transitions) {
        if (t.name == name) return true;
    }
    return false;
}

size_t Shape::transition_to(const std::wstring& name) {
    for (const auto& t : m_transitions) {
        if (t.name == name) return t.next_shape_index;
    }
    return SIZE_MAX;
}

ShapeRoot::ShapeRoot() {
    m_empty = std::make_shared<Shape>();
    m_empty->set_shape_index(0);
    m_shapes.push_back(m_empty);
}

ShapeRoot& ShapeRoot::instance() {
    static ShapeRoot root;
    return root;
}

std::shared_ptr<Shape> ShapeRoot::empty_shape() {
    return m_empty;
}

std::shared_ptr<Shape> ShapeRoot::get_or_create_transition(std::shared_ptr<Shape> current, const std::wstring& name) {
    size_t next_idx = current->transition_to(name);
    if (next_idx != SIZE_MAX && next_idx < m_shapes.size()) {
        return m_shapes[next_idx];
    }

    auto next = std::make_shared<Shape>();
    for (const auto& prop : current->property_chain()) {
        next->add_property(prop);
    }
    next->add_property(name);
    next->set_shape_index(m_shapes.size());

    current->add_transition(name, next->shape_index());

    m_shapes.push_back(next);
    return next;
}

void ShapeRoot::track_object_shape(JSObject* obj) {
    auto it = m_object_shape.find(obj);
    if (it != m_object_shape.end()) {
        // Remove from old shape
        auto shape_it = m_shape_map.find(it->second);
        if (shape_it != m_shape_map.end()) {
            shape_it->second.erase(obj);
        }
    }

    size_t gen = obj->shape_generation();
    if (gen >= m_shapes.size()) gen = 0;

    m_shape_map[gen].insert(obj);
    m_object_shape[obj] = gen;
    m_shapes[gen]->increment_object_count();
}

void ShapeRoot::untrack_object_shape(JSObject* obj) {
    auto it = m_object_shape.find(obj);
    if (it != m_object_shape.end()) {
        auto shape_it = m_shape_map.find(it->second);
        if (shape_it != m_shape_map.end()) {
            shape_it->second.erase(obj);
            m_shapes[it->second]->decrement_object_count();
        }
        m_object_shape.erase(it);
    }
}

} // namespace hjs::vm
