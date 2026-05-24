#include <hre/storage/indexeddb.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cctype>

namespace hre::storage {

// ---- idb_key_range --------------------------------------------------------

bool idb_key_range::includes(const std::wstring& key) const {
    if (!lower.empty()) {
        if (lower_bound == CLOSED && key < lower) return false;
        if (lower_bound == OPEN && key <= lower) return false;
    }
    if (!upper.empty()) {
        if (upper_bound == CLOSED && key > upper) return false;
        if (upper_bound == OPEN && key >= upper) return false;
    }
    return true;
}

idb_key_range idb_key_range::only(const std::wstring& key) {
    return { key, key, CLOSED, CLOSED };
}

idb_key_range idb_key_range::lower_bound_only(const std::wstring& key, bool open) {
    return { key, L"", open ? OPEN : CLOSED, NONE };
}

idb_key_range idb_key_range::upper_bound_only(const std::wstring& key, bool open) {
    return { L"", key, NONE, open ? OPEN : CLOSED };
}

idb_key_range idb_key_range::bound(const std::wstring& lower, const std::wstring& upper,
                                     bool lower_open, bool upper_open) {
    return { lower, upper, lower_open ? OPEN : CLOSED, upper_open ? OPEN : CLOSED };
}

// ---- idb_index ------------------------------------------------------------

void idb_index::add_entry(const std::wstring& index_key, const std::wstring& primary_key) {
    auto& entry = entries[index_key];
    entry.key = index_key;
    entry.primary_keys.insert(primary_key);
}

void idb_index::remove_entry(const std::wstring& index_key, const std::wstring& primary_key) {
    auto it = entries.find(index_key);
    if (it != entries.end()) {
        it->second.primary_keys.erase(primary_key);
        if (it->second.primary_keys.empty()) entries.erase(it);
    }
}

std::vector<std::wstring> idb_index::get_primary_keys(const std::wstring& index_key) const {
    auto it = entries.find(index_key);
    if (it == entries.end()) return {};
    return { it->second.primary_keys.begin(), it->second.primary_keys.end() };
}

// ---- idb_object_store -----------------------------------------------------

idb_object_store::idb_object_store(const std::wstring& name, const std::wstring& key_path)
    : m_name(name), m_key_path(key_path) {}

bool idb_object_store::put(const std::wstring& key, const std::wstring& value) {
    idb_object_record rec;
    rec.key = key;
    rec.value = value;
    rec.version = m_next_version++;
    m_records[key] = rec;

    for (auto& [_, index] : m_indexes) {
        index.add_entry(key, key);
    }
    return true;
}

bool idb_object_store::get(const std::wstring& key, std::wstring& value) const {
    auto it = m_records.find(key);
    if (it == m_records.end()) return false;
    value = it->second.value;
    return true;
}

bool idb_object_store::remove(const std::wstring& key) {
    auto it = m_records.find(key);
    if (it == m_records.end()) return false;
    for (auto& [_, index] : m_indexes) {
        index.remove_entry(key, key);
    }
    m_records.erase(it);
    return true;
}

void idb_object_store::clear() {
    m_records.clear();
    for (auto& [_, index] : m_indexes) index.entries.clear();
}

idb_index* idb_object_store::create_index(const std::wstring& name, const std::wstring& key_path,
                                           bool unique, bool multi_entry) {
    if (m_indexes.find(name) != m_indexes.end()) return nullptr;
    idb_index idx;
    idx.name = name;
    idx.key_path = key_path;
    idx.unique = unique;
    idx.multi_entry = multi_entry;
    auto result = m_indexes.emplace(name, idx);
    return &result.first->second;
}

idb_index* idb_object_store::get_index(const std::wstring& name) {
    auto it = m_indexes.find(name);
    return it != m_indexes.end() ? &it->second : nullptr;
}

const idb_index* idb_object_store::get_index(const std::wstring& name) const {
    auto it = m_indexes.find(name);
    return it != m_indexes.end() ? &it->second : nullptr;
}

void idb_object_store::delete_index(const std::wstring& name) {
    m_indexes.erase(name);
}

std::vector<std::wstring> idb_object_store::get_all_keys() const {
    std::vector<std::wstring> keys;
    for (const auto& [k, _] : m_records) keys.push_back(k);
    return keys;
}

std::vector<std::wstring> idb_object_store::get_all(const idb_key_range& range) const {
    std::vector<std::wstring> results;
    for (const auto& [k, rec] : m_records) {
        if (range.includes(k)) results.push_back(rec.value);
    }
    return results;
}

// ---- idb_cursor -----------------------------------------------------------

idb_cursor::idb_cursor(idb_object_store* store, direction dir, const idb_key_range& range)
    : m_store(store), m_dir(dir), m_range(range) {
    m_keys = store->get_all_keys();
    if (dir == PREV || dir == PREV_UNIQUE) {
        std::reverse(m_keys.begin(), m_keys.end());
    }
    m_position = 0;
    m_done = m_keys.empty();
    if (!m_done) {
        m_current_key = m_keys[0];
        m_store->get(m_current_key, m_current_value);
    }
}

bool idb_cursor::advance(int count) {
    m_position += count;
    if (m_position >= m_keys.size()) {
        m_done = true;
        return false;
    }
    m_current_key = m_keys[m_position];
    m_store->get(m_current_key, m_current_value);
    return true;
}

bool idb_cursor::continue_cursor(const std::wstring& key) {
    if (!key.empty()) {
        auto it = std::find(m_keys.begin(), m_keys.end(), key);
        if (it != m_keys.end()) {
            m_position = std::distance(m_keys.begin(), it) + 1;
        } else {
            m_done = true;
            return false;
        }
    } else {
        ++m_position;
    }

    if (m_position >= m_keys.size()) {
        m_done = true;
        return false;
    }
    m_current_key = m_keys[m_position];
    m_store->get(m_current_key, m_current_value);
    return true;
}

// ---- idb_database ---------------------------------------------------------

idb_database::idb_database(const std::wstring& name, int64_t version) {
    m_info.name = name;
    m_info.version = version;
}

idb_object_store* idb_database::create_object_store(const std::wstring& name, const std::wstring& key_path, bool auto_increment) {
    if (m_stores.find(name) != m_stores.end()) return nullptr;
    auto store = std::make_shared<idb_object_store>(name, key_path);
    m_stores[name] = store;
    m_info.max_object_store_id = static_cast<int64_t>(m_stores.size());
    return store.get();
}

void idb_database::delete_object_store(const std::wstring& name) {
    m_stores.erase(name);
}

idb_object_store* idb_database::get_object_store(const std::wstring& name) {
    auto it = m_stores.find(name);
    return it != m_stores.end() ? it->second.get() : nullptr;
}

const idb_object_store* idb_database::get_object_store(const std::wstring& name) const {
    auto it = m_stores.find(name);
    return it != m_stores.end() ? it->second.get() : nullptr;
}

bool idb_database::has_object_store(const std::wstring& name) const {
    return m_stores.find(name) != m_stores.end();
}

std::vector<std::wstring> idb_database::get_object_store_names() const {
    std::vector<std::wstring> names;
    for (const auto& [n, _] : m_stores) names.push_back(n);
    return names;
}

bool idb_database::save_to_file(const std::wstring& path) const {
    std::wofstream file(path);
    if (!file.is_open()) return false;
    file << L"{\"name\":\"" << m_info.name << L"\",\"version\":" << m_info.version;
    file << L",\"stores\":{";
    bool first_store = true;
    for (const auto& [sname, store] : m_stores) {
        if (!first_store) file << L",";
        file << L"\"" << sname << L"\":{";
        file << L"\"key_path\":\"" << store->key_path() << L"\"";

        file << L",\"records\":{";
        bool first_rec = true;
        for (const auto& [k, rec] : store->records()) {
            if (!first_rec) file << L",";
            file << L"\"" << rec.key << L"\":\"" << rec.value << L"\"";
            first_rec = false;
        }
        file << L"}}";
        first_store = false;
    }
    file << L"}}";
    return true;
}

bool idb_database::load_from_file(const std::wstring& path) {
    (void)path;
    return false;
}

// ---- indexeddb_engine -----------------------------------------------------

indexeddb_engine::indexeddb_engine() = default;
indexeddb_engine::~indexeddb_engine() = default;

indexeddb_engine& indexeddb_engine::instance() {
    static indexeddb_engine engine;
    return engine;
}

std::shared_ptr<idb_database> indexeddb_engine::open(const std::wstring& name, int64_t version) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_databases.find(name);
    if (it != m_databases.end()) return it->second;

    auto db = std::make_shared<idb_database>(name, version);
    m_databases[name] = db;

    std::wstring db_path = m_storage_path + name + L".json";
    std::wifstream file(db_path);
    if (file.is_open()) {
        db->load_from_file(db_path);
    }

    return db;
}

void indexeddb_engine::delete_database(const std::wstring& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_databases.erase(name);
}

bool indexeddb_engine::database_exists(const std::wstring& name) const {
    return m_databases.find(name) != m_databases.end();
}

std::vector<std::wstring> indexeddb_engine::get_database_names() const {
    std::vector<std::wstring> names;
    for (const auto& [n, _] : m_databases) names.push_back(n);
    return names;
}

int64_t indexeddb_engine::get_database_version(const std::wstring& name) const {
    auto it = m_databases.find(name);
    return it != m_databases.end() ? it->second->version() : 0;
}

uint64_t indexeddb_engine::get_total_storage_usage() const {
    uint64_t total = 0;
    for (const auto& [_, db] : m_databases) {
        for (const auto& sname : db->get_object_store_names()) {
            auto* store = db->get_object_store(sname);
            if (store) total += static_cast<uint64_t>(store->count()) * 1024;
        }
    }
    return total;
}

idb_object_store* indexeddb_engine::get_store_for_transaction(const std::wstring& db_name,
                                                                const std::wstring& store_name) {
    auto it = m_databases.find(db_name);
    if (it == m_databases.end()) return nullptr;
    return it->second->get_object_store(store_name);
}

} // namespace hre::storage
