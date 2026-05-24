#include <hjs/vm/vm.hpp>
#include <hjs/parser/parser.hpp>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <string>
#include <vector>

namespace hyperion::js {

struct MapEntry {
    value key;
    value value;
    bool deleted;
    MapEntry() : deleted(false) {}
};

class MapObject : public object {
public:
    std::vector<MapEntry> entries;
    std::map<std::wstring, size_t> key_index;
    size_t size;

    MapObject() : object{}, size(0) {
        object_type = ObjectType::Map;
    }
};

class SetObject : public object {
public:
    std::vector<value> values;
    std::set<std::wstring> key_set;
    size_t size;

    SetObject() : object{}, size(0) {
        object_type = ObjectType::Set;
    }
};

class WeakMapObject : public object {
public:
    std::vector<std::pair<object*, value>> entries;

    WeakMapObject() : object{} {
        object_type = ObjectType::WeakMap;
    }
};

class WeakSetObject : public object {
public:
    std::vector<object*> values;

    WeakSetObject() : object{} {
        object_type = ObjectType::WeakSet;
    }
};

std::wstring value_to_key_string(const value& v) {
    if (v.is_string()) return v.as_string();
    if (v.is_number()) return std::to_wstring(v.as_number());
    if (v.is_boolean()) return v.as_boolean() ? L"true" : L"false";
    if (v.is_null()) return L"null";
    if (v.is_undefined()) return L"undefined";
    if (v.is_object()) {
        return L"object:" + std::to_wstring(reinterpret_cast<uintptr_t>(v.as_object()));
    }
    return L"unknown";
}

value vm::create_map() {
    auto* map = m_gc.allocate<MapObject>();
    map->entries.reserve(16);
    return value::object_ptr(map);
}

value vm::map_set(const value& map_val, const value& key, const value& val) {
    if (!map_val.is_object()) return value::undefined();

    auto* map_obj = static_cast<MapObject*>(map_val.as_object());

    std::wstring key_str = value_to_key_string(key);

    auto it = map_obj->key_index.find(key_str);
    if (it != map_obj->key_index.end()) {
        size_t idx = it->second;
        if (!map_obj->entries[idx].deleted) {
            map_obj->entries[idx].value = val;
            return map_val;
        }
    }

    MapEntry entry;
    entry.key = key;
    entry.value = val;
    entry.deleted = false;

    size_t idx = map_obj->entries.size();
    map_obj->entries.push_back(entry);
    map_obj->key_index[key_str] = idx;
    map_obj->size++;

    return map_val;
}

value vm::map_get(const value& map_val, const value& key) {
    if (!map_val.is_object()) return value::undefined();

    auto* map_obj = static_cast<MapObject*>(map_val.as_object());

    std::wstring key_str = value_to_key_string(key);

    auto it = map_obj->key_index.find(key_str);
    if (it != map_obj->key_index.end()) {
        size_t idx = it->second;
        if (!map_obj->entries[idx].deleted) {
            return map_obj->entries[idx].value;
        }
    }

    return value::undefined();
}

value vm::map_has(const value& map_val, const value& key) {
    if (!map_val.is_object()) return value::boolean(false);

    auto* map_obj = static_cast<MapObject*>(map_val.as_object());

    std::wstring key_str = value_to_key_string(key);

    auto it = map_obj->key_index.find(key_str);
    if (it != map_obj->key_index.end()) {
        return value::boolean(!map_obj->entries[it->second].deleted);
    }

    return value::boolean(false);
}

value vm::map_delete(const value& map_val, const value& key) {
    if (!map_val.is_object()) return value::boolean(false);

    auto* map_obj = static_cast<MapObject*>(map_val.as_object());

    std::wstring key_str = value_to_key_string(key);

    auto it = map_obj->key_index.find(key_str);
    if (it != map_obj->key_index.end()) {
        size_t idx = it->second;
        if (!map_obj->entries[idx].deleted) {
            map_obj->entries[idx].deleted = true;
            map_obj->size--;
            return value::boolean(true);
        }
    }

    return value::boolean(false);
}

value vm::map_clear(const value& map_val) {
    if (!map_val.is_object()) return value::undefined();

    auto* map_obj = static_cast<MapObject*>(map_val.as_object());
    map_obj->entries.clear();
    map_obj->key_index.clear();
    map_obj->size = 0;

    return value::undefined();
}

value vm::map_size(const value& map_val) {
    if (!map_val.is_object()) return value::number(0);

    auto* map_obj = static_cast<MapObject*>(map_val.as_object());
    return value::number(static_cast<double>(map_obj->size));
}

value vm::create_set() {
    auto* set = m_gc.allocate<SetObject>();
    set->values.reserve(16);
    return value::object_ptr(set);
}

value vm::set_add(const value& set_val, const value& key) {
    if (!set_val.is_object()) return value::undefined();

    auto* set_obj = static_cast<SetObject*>(set_val.as_object());

    std::wstring key_str = value_to_key_string(key);

    if (set_obj->key_set.find(key_str) != set_obj->key_set.end()) {
        return set_val;
    }

    set_obj->values.push_back(key);
    set_obj->key_set.insert(key_str);
    set_obj->size++;

    return set_val;
}

value vm::set_has(const value& set_val, const value& key) {
    if (!set_val.is_object()) return value::boolean(false);

    auto* set_obj = static_cast<SetObject*>(set_val.as_object());

    std::wstring key_str = value_to_key_string(key);

    return value::boolean(set_obj->key_set.find(key_str) != set_obj->key_set.end());
}

value vm::set_delete(const value& set_val, const value& key) {
    if (!set_val.is_object()) return value::boolean(false);

    auto* set_obj = static_cast<SetObject*>(set_val.as_object());

    std::wstring key_str = value_to_key_string(key);

    auto it = set_obj->key_set.find(key_str);
    if (it != set_obj->key_set.end()) {
        set_obj->key_set.erase(it);
        set_obj->size--;
        return value::boolean(true);
    }

    return value::boolean(false);
}

value vm::set_clear(const value& set_val) {
    if (!set_val.is_object()) return value::undefined();

    auto* set_obj = static_cast<SetObject*>(set_val.as_object());
    set_obj->values.clear();
    set_obj->key_set.clear();
    set_obj->size = 0;

    return value::undefined();
}

value vm::set_size(const value& set_val) {
    if (!set_val.is_object()) return value::number(0);

    auto* set_obj = static_cast<SetObject*>(set_val.as_object());
    return value::number(static_cast<double>(set_obj->size));
}

value vm::create_weakmap() {
    auto* weakmap = m_gc.allocate<WeakMapObject>();
    return value::object_ptr(weakmap);
}

value vm::weakmap_set(const value& weakmap_val, const value& key, const value& val) {
    if (!weakmap_val.is_object() || !key.is_object()) return value::undefined();

    auto* wm = static_cast<WeakMapObject*>(weakmap_val.as_object());
    auto* key_obj = key.as_object();

    for (auto& entry : wm->entries) {
        if (entry.first == key_obj) {
            entry.second = val;
            return weakmap_val;
        }
    }

    wm->entries.push_back({key_obj, val});
    return weakmap_val;
}

value vm::weakmap_get(const value& weakmap_val, const value& key) {
    if (!weakmap_val.is_object() || !key.is_object()) return value::undefined();

    auto* wm = static_cast<WeakMapObject*>(weakmap_val.as_object());
    auto* key_obj = key.as_object();

    for (auto& entry : wm->entries) {
        if (entry.first == key_obj) {
            return entry.second;
        }
    }

    return value::undefined();
}

value vm::weakmap_has(const value& weakmap_val, const value& key) {
    if (!weakmap_val.is_object() || !key.is_object()) return value::boolean(false);

    auto* wm = static_cast<WeakMapObject*>(weakmap_val.as_object());
    auto* key_obj = key.as_object();

    for (auto& entry : wm->entries) {
        if (entry.first == key_obj) {
            return value::boolean(true);
        }
    }

    return value::boolean(false);
}

value vm::weakmap_delete(const value& weakmap_val, const value& key) {
    if (!weakmap_val.is_object() || !key.is_object()) return value::boolean(false);

    auto* wm = static_cast<WeakMapObject*>(weakmap_val.as_object());
    auto* key_obj = key.as_object();

    for (auto it = wm->entries.begin(); it != wm->entries.end(); ++it) {
        if (it->first == key_obj) {
            wm->entries.erase(it);
            return value::boolean(true);
        }
    }

    return value::boolean(false);
}

value vm::create_weakset() {
    auto* weakset = m_gc.allocate<WeakSetObject>();
    return value::object_ptr(weakset);
}

value vm::weakset_add(const value& weakset_val, const value& key) {
    if (!weakset_val.is_object() || !key.is_object()) return value::undefined();

    auto* ws = static_cast<WeakSetObject*>(weakset_val.as_object());
    auto* key_obj = key.as_object();

    for (auto* v : ws->values) {
        if (v == key_obj) return weakset_val;
    }

    ws->values.push_back(key_obj);
    return weakset_val;
}

value vm::weakset_has(const value& weakset_val, const value& key) {
    if (!weakset_val.is_object() || !key.is_object()) return value::boolean(false);

    auto* ws = static_cast<WeakSetObject*>(weakset_val.as_object());
    auto* key_obj = key.as_object();

    for (auto* v : ws->values) {
        if (v == key_obj) return value::boolean(true);
    }

    return value::boolean(false);
}

value vm::weakset_delete(const value& weakset_val, const value& key) {
    if (!weakset_val.is_object() || !key.is_object()) return value::boolean(false);

    auto* ws = static_cast<WeakSetObject*>(weakset_val.as_object());
    auto* key_obj = key.as_object();

    for (auto it = ws->values.begin(); it != ws->values.end(); ++it) {
        if (*it == key_obj) {
            ws->values.erase(it);
            return value::boolean(true);
        }
    }

    return value::boolean(false);
}

void vm::init_map_set_prototypes() {
    m_map_prototype = create_object();

    set_native_func(m_map_prototype, "get", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::undefined();
        return map_get(args[0], args[1]);
    });

    set_native_func(m_map_prototype, "set", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return map_set(args[0], args[1], args[2]);
    });

    set_native_func(m_map_prototype, "has", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return map_has(args[0], args[1]);
    });

    set_native_func(m_map_prototype, "delete", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return map_delete(args[0], args[1]);
    });

    set_native_func(m_map_prototype, "clear", [this](const std::vector<value>& args) {
        if (args.empty()) return value::undefined();
        return map_clear(args[0]);
    });

    set_native_func(m_map_prototype, "size", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return map_size(args[0]);
    });

    set_native_func(m_map_prototype, "keys", [this](const std::vector<value>& args) {
        if (args.empty() || !args[0].is_object()) return value::null();
        auto* map_obj = static_cast<MapObject*>(args[0].as_object());
        auto* arr = m_gc.allocate<object>();
        arr->object_type = ObjectType::Array;

        size_t idx = 0;
        for (auto& entry : map_obj->entries) {
            if (!entry.deleted) {
                arr->properties[std::to_wstring(idx++)] = entry.key;
            }
        }
        arr->properties[L"length"] = value::number(static_cast<double>(idx));

        auto* iter = m_gc.allocate<object>();
        iter->object_type = ObjectType::Iterator;
        iter->properties[L"data"] = value::object_ptr(arr);
        iter->properties[L"index"] = value::number(0);

        set_native_func(iter, "next", [this](const std::vector<value>& args) {
            if (args.empty() || !args[0].is_object()) return value::null();
            auto* iter_obj = args[0].as_object();
            auto* arr_obj = static_cast<object*>(iter_obj->properties[L"data"].as_object());
            double idx = iter_obj->properties[L"index"].as_number();
            double len = arr_obj->properties[L"length"].as_number();

            auto* result = m_gc.allocate<object>();
            result->properties[L"done"] = value::boolean(idx >= len);

            if (idx < len) {
                result->properties[L"value"] = arr->properties[std::to_wstring(static_cast<size_t>(idx))];
                iter_obj->properties[L"index"] = value::number(idx + 1);
            }

            return value::object_ptr(result);
        });

        return value::object_ptr(iter);
    });

    set_native_func(m_map_prototype, "values", [this](const std::vector<value>& args) {
        if (args.empty() || !args[0].is_object()) return value::null();
        auto* map_obj = static_cast<MapObject*>(args[0].as_object());
        auto* arr = m_gc.allocate<object>();
        arr->object_type = ObjectType::Array;

        size_t idx = 0;
        for (auto& entry : map_obj->entries) {
            if (!entry.deleted) {
                arr->properties[std::to_wstring(idx++)] = entry.value;
            }
        }
        arr->properties[L"length"] = value::number(static_cast<double>(idx));

        auto* iter = m_gc.allocate<object>();
        iter->object_type = ObjectType::Iterator;
        iter->properties[L"data"] = value::object_ptr(arr);
        iter->properties[L"index"] = value::number(0);

        set_native_func(iter, "next", [this](const std::vector<value>& args) {
            if (args.empty() || !args[0].is_object()) return value::null();
            auto* iter_obj = args[0].as_object();
            auto* arr_obj = static_cast<object*>(iter_obj->properties[L"data"].as_object());
            double idx = iter_obj->properties[L"index"].as_number();
            double len = arr_obj->properties[L"length"].as_number();

            auto* result = m_gc.allocate<object>();
            result->properties[L"done"] = value::boolean(idx >= len);

            if (idx < len) {
                result->properties[L"value"] = arr_obj->properties[std::to_wstring(static_cast<size_t>(idx))];
                iter_obj->properties[L"index"] = value::number(idx + 1);
            }

            return value::object_ptr(result);
        });

        return value::object_ptr(iter);
    });

    set_native_func(m_map_prototype, "entries", [this](const std::vector<value>& args) {
        if (args.empty() || !args[0].is_object()) return value::null();
        auto* map_obj = static_cast<MapObject*>(args[0].as_object());

        auto* iter = m_gc.allocate<object>();
        iter->object_type = ObjectType::Iterator;
        iter->properties[L"map"] = args[0];
        iter->properties[L"index"] = value::number(0);

        set_native_func(iter, "next", [this](const std::vector<value>& args) {
            if (args.empty() || !args[0].is_object()) return value::null();
            auto* iter_obj = args[0].as_object();
            auto* map_obj = static_cast<MapObject*>(iter_obj->properties[L"map"].as_object());
            double idx = iter_obj->properties[L"index"].as_number();

            auto* result = m_gc.allocate<object>();
            size_t entry_idx = static_cast<size_t>(idx);

            result->properties[L"done"] = value::boolean(entry_idx >= map_obj->entries.size());

            if (entry_idx < map_obj->entries.size()) {
                auto& entry = map_obj->entries[entry_idx];
                auto* entry_arr = m_gc.allocate<object>();
                entry_arr->object_type = ObjectType::Array;
                entry_arr->properties[L"0"] = entry.key;
                entry_arr->properties[L"1"] = entry.value;
                entry_arr->properties[L"length"] = value::number(2);

                result->properties[L"value"] = value::object_ptr(entry_arr);
                iter_obj->properties[L"index"] = value::number(idx + 1);
            }

            return value::object_ptr(result);
        });

        return value::object_ptr(iter);
    });

    m_set_prototype = create_object();

    set_native_func(m_set_prototype, "add", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::undefined();
        return set_add(args[0], args[1]);
    });

    set_native_func(m_set_prototype, "has", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return set_has(args[0], args[1]);
    });

    set_native_func(m_set_prototype, "delete", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return set_delete(args[0], args[1]);
    });

    set_native_func(m_set_prototype, "clear", [this](const std::vector<value>& args) {
        if (args.empty()) return value::undefined();
        return set_clear(args[0]);
    });

    set_native_func(m_set_prototype, "size", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return set_size(args[0]);
    });

    set_native_func(m_set_prototype, "values", [this](const std::vector<value>& args) {
        if (args.empty() || !args[0].is_object()) return value::null();
        auto* set_obj = static_cast<SetObject*>(args[0].as_object());
        auto* arr = m_gc.allocate<object>();
        arr->object_type = ObjectType::Array;

        for (size_t i = 0; i < set_obj->values.size(); ++i) {
            arr->properties[std::to_wstring(i)] = set_obj->values[i];
        }
        arr->properties[L"length"] = value::number(static_cast<double>(set_obj->values.size()));

        auto* iter = m_gc.allocate<object>();
        iter->object_type = ObjectType::Iterator;
        iter->properties[L"data"] = value::object_ptr(arr);
        iter->properties[L"index"] = value::number(0);

        set_native_func(iter, "next", [this](const std::vector<value>& args) {
            if (args.empty() || !args[0].is_object()) return value::null();
            auto* iter_obj = args[0].as_object();
            auto* arr_obj = static_cast<object*>(iter_obj->properties[L"data"].as_object());
            double idx = iter_obj->properties[L"index"].as_number();
            double len = arr_obj->properties[L"length"].as_number();

            auto* result = m_gc.allocate<object>();
            result->properties[L"done"] = value::boolean(idx >= len);

            if (idx < len) {
                result->properties[L"value"] = arr_obj->properties[std::to_wstring(static_cast<size_t>(idx))];
                iter_obj->properties[L"index"] = value::number(idx + 1);
            }

            return value::object_ptr(result);
        });

        return value::object_ptr(iter);
    });

    m_weakmap_prototype = create_object();

    set_native_func(m_weakmap_prototype, "get", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::undefined();
        return weakmap_get(args[0], args[1]);
    });

    set_native_func(m_weakmap_prototype, "set", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return weakmap_set(args[0], args[1], args[2]);
    });

    set_native_func(m_weakmap_prototype, "has", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return weakmap_has(args[0], args[1]);
    });

    set_native_func(m_weakmap_prototype, "delete", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return weakmap_delete(args[0], args[1]);
    });

    m_weakset_prototype = create_object();

    set_native_func(m_weakset_prototype, "add", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::undefined();
        return weakset_add(args[0], args[1]);
    });

    set_native_func(m_weakset_prototype, "has", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return weakset_has(args[0], args[1]);
    });

    set_native_func(m_weakset_prototype, "delete", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::boolean(false);
        return weakset_delete(args[0], args[1]);
    });
}

}