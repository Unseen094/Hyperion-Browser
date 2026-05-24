#pragma once

#include <cstdint>
#include <unordered_map>
#include <list>
#include <optional>
#include <functional>

namespace hre::net {

template<typename K, typename V>
class lru_cache {
public:
    using evict_callback = std::function<void(const K&, const V&)>;

    explicit lru_cache(size_t max_entries = 1000)
        : m_max_entries(max_entries) {}

    void put(const K& key, const V& value) {
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            it->second->second = value;
            m_list.splice(m_list.begin(), m_list, it->second);
            return;
        }
        if (m_map.size() >= m_max_entries) {
            evict_one();
        }
        m_list.emplace_front(key, value);
        m_map[key] = m_list.begin();
    }

    std::optional<V> get(const K& key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return std::nullopt;
        }
        m_list.splice(m_list.begin(), m_list, it->second);
        return it->second->second;
    }

    bool contains(const K& key) const {
        return m_map.find(key) != m_map.end();
    }

    void remove(const K& key) {
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            m_list.erase(it->second);
            m_map.erase(it);
        }
    }

    void clear() {
        m_list.clear();
        m_map.clear();
    }

    size_t size() const { return m_map.size(); }
    size_t capacity() const { return m_max_entries; }
    void set_capacity(size_t max_entries) {
        m_max_entries = max_entries;
        while (m_map.size() > m_max_entries) {
            evict_one();
        }
    }

    void set_evict_callback(evict_callback cb) { m_evict_cb = cb; }

private:
    using list_type = std::list<std::pair<K, V>>;
    using iterator_type = typename list_type::iterator;

    size_t m_max_entries;
    list_type m_list;
    std::unordered_map<K, iterator_type> m_map;
    evict_callback m_evict_cb;

    void evict_one() {
        if (m_list.empty()) return;
        auto last = std::prev(m_list.end());
        if (m_evict_cb) {
            m_evict_cb(last->first, last->second);
        }
        m_map.erase(last->first);
        m_list.pop_back();
    }
};

} // namespace hre::net
