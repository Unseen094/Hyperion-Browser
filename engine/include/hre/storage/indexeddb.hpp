#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>

namespace hre::storage {

// ---- Key Range ------------------------------------------------------------

struct idb_key_range {
    enum bound_type { NONE, OPEN, CLOSED };

    std::wstring lower;
    std::wstring upper;
    bound_type lower_bound = NONE;
    bound_type upper_bound = NONE;

    bool includes(const std::wstring& key) const;
    static idb_key_range only(const std::wstring& key);
    static idb_key_range lower_bound_only(const std::wstring& key, bool open = false);
    static idb_key_range upper_bound_only(const std::wstring& key, bool open = false);
    static idb_key_range bound(const std::wstring& lower, const std::wstring& upper, bool lower_open = false, bool upper_open = false);
};

// ---- Index ----------------------------------------------------------------

struct idb_index_entry {
    std::wstring key;
    std::set<std::wstring> primary_keys; // multiple primary keys can have same index value
};

struct idb_index {
    std::wstring name;
    std::wstring key_path;
    bool unique = false;
    bool multi_entry = false;
    std::map<std::wstring, idb_index_entry> entries; // index_key -> entry

    void add_entry(const std::wstring& index_key, const std::wstring& primary_key);
    void remove_entry(const std::wstring& index_key, const std::wstring& primary_key);
    std::vector<std::wstring> get_primary_keys(const std::wstring& index_key) const;
};

// ---- Object Store ---------------------------------------------------------

struct idb_object_record {
    std::wstring key;
    std::wstring value; // serialized
    int64_t version = 0;
};

class idb_object_store {
public:
    idb_object_store(const std::wstring& name, const std::wstring& key_path = L"id");

    // CRUD
    bool put(const std::wstring& key, const std::wstring& value);
    bool get(const std::wstring& key, std::wstring& value) const;
    bool remove(const std::wstring& key);
    void clear();
    int64_t count() const { return static_cast<int64_t>(m_records.size()); }
    bool has(const std::wstring& key) const { return m_records.find(key) != m_records.end(); }

    // Indexes
    idb_index* create_index(const std::wstring& name, const std::wstring& key_path, bool unique = false, bool multi_entry = false);
    idb_index* get_index(const std::wstring& name);
    const idb_index* get_index(const std::wstring& name) const;
    void delete_index(const std::wstring& name);

    // Cursor iteration
    const std::map<std::wstring, idb_object_record>& records() const { return m_records; }
    std::vector<std::wstring> get_all_keys() const;
    std::vector<std::wstring> get_all(const idb_key_range& range = {}) const;

    std::wstring name() const { return m_name; }
    std::wstring key_path() const { return m_key_path; }

private:
    std::wstring m_name;
    std::wstring m_key_path;
    std::map<std::wstring, idb_object_record> m_records;
    std::map<std::wstring, idb_index> m_indexes;
    int64_t m_next_version = 1;
};

// ---- Object Store Cursor --------------------------------------------------

class idb_cursor {
public:
    enum direction { NEXT, PREV, NEXT_UNIQUE, PREV_UNIQUE };

    idb_cursor(idb_object_store* store, direction dir = NEXT, const idb_key_range& range = {});

    bool advance(int count = 1);
    bool continue_cursor(const std::wstring& key = L"");

    std::wstring key() const { return m_current_key; }
    std::wstring value() const { return m_current_value; }
    bool done() const { return m_done; }

private:
    idb_object_store* m_store;
    direction m_dir;
    idb_key_range m_range;
    std::vector<std::wstring> m_keys;
    size_t m_position = 0;
    std::wstring m_current_key;
    std::wstring m_current_value;
    bool m_done = false;
};

// ---- Database -------------------------------------------------------------

struct idb_database_info {
    std::wstring name;
    int64_t version = 1;
    int64_t max_object_store_id = 0;
};

class idb_database {
public:
    idb_database(const std::wstring& name, int64_t version = 1);

    idb_object_store* create_object_store(const std::wstring& name, const std::wstring& key_path = L"id", bool auto_increment = false);
    void delete_object_store(const std::wstring& name);
    idb_object_store* get_object_store(const std::wstring& name);
    const idb_object_store* get_object_store(const std::wstring& name) const;

    bool has_object_store(const std::wstring& name) const;
    std::vector<std::wstring> get_object_store_names() const;

    std::wstring name() const { return m_info.name; }
    int64_t version() const { return m_info.version; }
    void set_version(int64_t v) { m_info.version = v; }

    // Serialization
    bool save_to_file(const std::wstring& path) const;
    bool load_from_file(const std::wstring& path);

private:
    idb_database_info m_info;
    std::map<std::wstring, std::shared_ptr<idb_object_store>> m_stores;
};

// ---- IndexedDB Engine -----------------------------------------------------

class indexeddb_engine {
public:
    indexeddb_engine();
    ~indexeddb_engine();

    static indexeddb_engine& instance();

    // Database lifecycle
    std::shared_ptr<idb_database> open(const std::wstring& name, int64_t version = 1);
    void delete_database(const std::wstring& name);
    bool database_exists(const std::wstring& name) const;

    // Metadata
    std::vector<std::wstring> get_database_names() const;
    int64_t get_database_version(const std::wstring& name) const;

    // Storage management
    void set_storage_path(const std::wstring& path) { m_storage_path = path; }
    std::wstring storage_path() const { return m_storage_path; }
    uint64_t get_total_storage_usage() const;

    // Quota callback
    using quota_check_callback = std::function<bool(const std::wstring& db_name, uint64_t estimated_size)>;
    void set_quota_check_callback(quota_check_callback cb) { m_quota_cb = cb; }

    // Read-only API for transactions
    idb_object_store* get_store_for_transaction(const std::wstring& db_name, const std::wstring& store_name);

private:
    std::map<std::wstring, std::shared_ptr<idb_database>> m_databases;
    std::wstring m_storage_path = L"./data/indexeddb/";
    quota_check_callback m_quota_cb;
    std::mutex m_mutex;
};

} // namespace hre::storage
