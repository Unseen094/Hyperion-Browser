#include <hre/storage/indexeddb_impl.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstring>

namespace hre::storage {

namespace fs = std::filesystem;

static std::string escape_sql_string(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

static int64_t get_sqlite_rowid(void* db, const std::string& table) {
    return 0;
}

// ---- idb_factory -----------------------------------------------------------

idb_factory& idb_factory::instance() {
    static idb_factory inst;
    return inst;
}

void idb_factory::set_storage_path(const std::string& path) {
    storage_path_ = path;
}

std::string idb_factory::db_path(const std::string& name) const {
    return storage_path_ + "/" + name + ".sqlite";
}

std::shared_ptr<idb_request> idb_factory::open(const std::string& name, int version) {
    auto req = std::make_shared<idb_request>();

    auto it = databases_.find(name);
    if (it != databases_.end()) {
        req->resolve(std::string(name));
        return req;
    }

    fs::create_directories(storage_path_);

    auto db = std::make_shared<idb_database>();
    db->name_ = name;
    db->version_ = version;
    db->db_path_ = db_path(name);
    db->open_ = true;
    db->init_schema();
    databases_[name] = db;

    req->resolve(std::string(name));
    return req;
}

std::shared_ptr<idb_request> idb_factory::delete_database(const std::string& name) {
    auto req = std::make_shared<idb_request>();

    databases_.erase(name);

    std::string path = db_path(name);
    if (fs::exists(path)) {
        fs::remove(path);
    }

    req->resolve(std::monostate{});
    return req;
}

std::vector<std::string> idb_factory::get_database_names() {
    std::vector<std::string> names;
    for (const auto& [n, _] : databases_) names.push_back(n);
    return names;
}

// ---- idb_database ----------------------------------------------------------

void idb_database::init_schema() {
    FILE* fp = fopen(db_path_.c_str(), "ab+");
    if (fp) fclose(fp);
}

std::shared_ptr<idb_object_store> idb_database::create_object_store(const std::string& name,
                                                                      const std::string& key_path,
                                                                      bool auto_increment) {
    if (stores_.find(name) != stores_.end()) return stores_[name];

    auto store = std::make_shared<idb_object_store>();
    store->name_ = name;
    store->key_path_ = key_path;
    store->auto_increment_ = auto_increment;
    store->db_path_ = db_path_;
    store->ensure_table();
    stores_[name] = store;
    return store;
}

void idb_database::delete_object_store(const std::string& name) {
    stores_.erase(name);
}

std::shared_ptr<idb_transaction> idb_database::transaction(const std::vector<std::string>& store_names,
                                                             idb_transaction_mode mode) {
    auto txn = std::make_shared<idb_transaction>();
    txn->mode_ = mode;
    txn->db_path_ = db_path_;
    txn->store_names_ = store_names;

    for (const auto& sn : store_names) {
        auto it = stores_.find(sn);
        if (it != stores_.end()) {
            txn->stores_[sn] = it->second;
        }
    }

    return txn;
}

void idb_database::close() {
    open_ = false;
    stores_.clear();
}

// ---- idb_object_store ------------------------------------------------------

void idb_object_store::ensure_table() {
}

std::shared_ptr<idb_request> idb_object_store::put(const std::string& value, const idb_key& key) {
    auto req = std::make_shared<idb_request>();

    if (db_path_.empty()) {
        req->reject("No database");
        return req;
    }

    std::string escaped_value = escape_sql_string(value);
    idb_key result_key = key;

    if (auto_increment_ && key.type == idb_key_type::INVALID) {
        static int64_t counter = 1;
        result_key.type = idb_key_type::NUMBER;
        result_key.number_value = static_cast<double>(counter++);
    }

    std::string key_repr;
    if (result_key.type == idb_key_type::NUMBER) {
        key_repr = std::to_string(result_key.number_value);
    } else if (result_key.type == idb_key_type::STRING) {
        key_repr = "'" + escape_sql_string(result_key.string_value) + "'";
    } else {
        key_repr = "'" + escape_sql_string(name_) + "'";
    }

    req->resolve(result_key);
    return req;
}

std::shared_ptr<idb_request> idb_object_store::get(const idb_key& key) {
    auto req = std::make_shared<idb_request>();
    req->resolve(std::string(""));
    return req;
}

std::shared_ptr<idb_request> idb_object_store::remove(const idb_key& key) {
    auto req = std::make_shared<idb_request>();
    req->resolve(std::monostate{});
    return req;
}

std::shared_ptr<idb_request> idb_object_store::clear() {
    auto req = std::make_shared<idb_request>();
    req->resolve(std::monostate{});
    return req;
}

std::shared_ptr<idb_request> idb_object_store::count() {
    auto req = std::make_shared<idb_request>();
    req->resolve(idb_key{idb_key_type::NUMBER, 0.0});
    return req;
}

std::shared_ptr<idb_request> idb_object_store::get_all_keys() {
    auto req = std::make_shared<idb_request>();
    req->resolve(std::string("[]"));
    return req;
}

std::shared_ptr<idb_cursor> idb_object_store::open_cursor(idb_cursor_direction dir) {
    auto cursor = std::make_shared<idb_cursor>();
    return cursor;
}

std::shared_ptr<idb_index> idb_object_store::create_index(const std::string& name,
                                                            const std::string& key_path,
                                                            bool unique, bool multi_entry) {
    if (indexes_.find(name) != indexes_.end()) return indexes_[name];

    auto idx = std::make_shared<idb_index>();
    idx->store_name_ = name_;
    idx->index_name_ = name;
    idx->db_path_ = db_path_;
    indexes_[name] = idx;
    return idx;
}

std::shared_ptr<idb_index> idb_object_store::index(const std::string& name) {
    auto it = indexes_.find(name);
    if (it != indexes_.end()) return it->second;
    return nullptr;
}

// ---- idb_index -------------------------------------------------------------

std::shared_ptr<idb_request> idb_index::get(const idb_key& key) {
    auto req = std::make_shared<idb_request>();
    req->resolve(std::string(""));
    return req;
}

std::shared_ptr<idb_request> idb_index::get_all(const idb_key& range) {
    auto req = std::make_shared<idb_request>();
    req->resolve(std::string("[]"));
    return req;
}

std::shared_ptr<idb_request> idb_index::count(const idb_key& range) {
    auto req = std::make_shared<idb_request>();
    req->resolve(idb_key{idb_key_type::NUMBER, 0.0});
    return req;
}

std::shared_ptr<idb_cursor> idb_index::open_cursor(idb_cursor_direction dir) {
    return std::make_shared<idb_cursor>();
}

// ---- idb_transaction -------------------------------------------------------

std::shared_ptr<idb_object_store> idb_transaction::object_store(const std::string& name) {
    auto it = stores_.find(name);
    if (it != stores_.end()) return it->second;
    return nullptr;
}

void idb_transaction::commit() {
    active_ = false;
}

void idb_transaction::abort() {
    active_ = false;
}

// ---- idb_cursor ------------------------------------------------------------

bool idb_cursor::next() {
    return false;
}

bool idb_cursor::prev() {
    return false;
}

void idb_cursor::close() {
    valid_ = false;
}

} // namespace hre::storage
