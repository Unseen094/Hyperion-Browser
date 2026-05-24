#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <variant>
#include <cstdint>

namespace hre::storage {

enum class idb_cursor_direction {
    NEXT,
    NEXT_UNIQUE,
    PREV,
    PREV_UNIQUE
};

enum class idb_transaction_mode {
    READ_ONLY,
    READ_WRITE,
    VERSION_CHANGE
};

enum class idb_key_type {
    NUMBER,
    STRING,
    DATE,
    BINARY,
    INVALID
};

struct idb_key {
    idb_key_type type = idb_key_type::INVALID;
    double number_value = 0;
    std::string string_value;
    std::vector<uint8_t> binary_value;

    bool operator<(const idb_key& o) const {
        if (type != o.type) return type < o.type;
        switch (type) {
            case idb_key_type::NUMBER: return number_value < o.number_value;
            case idb_key_type::STRING: return string_value < o.string_value;
            case idb_key_type::BINARY: return binary_value < o.binary_value;
            default: return false;
        }
    }
};

struct idb_index_meta {
    std::string name;
    std::string key_path;
    bool unique = false;
    bool multi_entry = false;
};

struct idb_object_store_meta {
    std::string name;
    std::string key_path;
    bool auto_increment = false;
    std::vector<idb_index_meta> indexes;
};

struct idb_database_meta {
    std::string name;
    int version = 1;
    std::vector<idb_object_store_meta> stores;
};

class idb_request {
public:
    using success_cb = std::function<void(const std::variant<std::monostate, idb_key, std::string>&)>;
    using error_cb = std::function<void(const std::string&)>;

    void on_success(success_cb cb) { success_cb_ = std::move(cb); }
    void on_error(error_cb cb) { error_cb_ = std::move(cb); }

    void resolve(const std::variant<std::monostate, idb_key, std::string>& result) {
        if (success_cb_) success_cb_(result);
    }

    void reject(const std::string& reason) {
        if (error_cb_) error_cb_(reason);
    }

    bool done = false;
private:
    success_cb success_cb_;
    error_cb error_cb_;
};

class idb_cursor {
public:
    idb_cursor() = default;

    bool valid() const { return valid_; }
    idb_key key() const { return key_; }
    idb_key primary_key() const { return primary_key_; }
    std::string value() const { return value_; }

    bool next();
    bool prev();
    void close();

private:
    friend class idb_object_store;
    idb_key key_;
    idb_key primary_key_;
    std::string value_;
    bool valid_ = false;
    int64_t sqlite_cursor_ = 0;
};

class idb_index {
public:
    idb_index() = default;

    std::shared_ptr<idb_request> get(const idb_key& key);
    std::shared_ptr<idb_request> get_all(const idb_key& range = {});
    std::shared_ptr<idb_request> count(const idb_key& range = {});
    std::shared_ptr<idb_cursor> open_cursor(idb_cursor_direction dir = idb_cursor_direction::NEXT);

private:
    friend class idb_object_store;
    std::string store_name_;
    std::string index_name_;
    std::string db_path_;
};

class idb_object_store {
public:
    idb_object_store() = default;

    std::shared_ptr<idb_request> put(const std::string& value, const idb_key& key = {});
    std::shared_ptr<idb_request> get(const idb_key& key);
    std::shared_ptr<idb_request> remove(const idb_key& key);
    std::shared_ptr<idb_request> clear();
    std::shared_ptr<idb_request> count();
    std::shared_ptr<idb_request> get_all_keys();
    std::shared_ptr<idb_cursor> open_cursor(idb_cursor_direction dir = idb_cursor_direction::NEXT);
    std::shared_ptr<idb_index> create_index(const std::string& name, const std::string& key_path,
                                             bool unique = false, bool multi_entry = false);
    std::shared_ptr<idb_index> index(const std::string& name);

    std::string name() const { return name_; }

private:
    friend class idb_database;
    std::string name_;
    std::string key_path_;
    bool auto_increment_ = false;
    std::string db_path_;
    std::map<std::string, std::shared_ptr<idb_index>> indexes_;

    void ensure_table();
    int64_t last_insert_rowid();
};

class idb_transaction {
public:
    idb_transaction() = default;

    std::shared_ptr<idb_object_store> object_store(const std::string& name);
    void commit();
    void abort();

    idb_transaction_mode mode() const { return mode_; }

private:
    friend class idb_database;
    idb_transaction_mode mode_ = idb_transaction_mode::READ_ONLY;
    std::string db_path_;
    std::vector<std::string> store_names_;
    std::map<std::string, std::shared_ptr<idb_object_store>> stores_;
    bool active_ = true;
};

class idb_database {
public:
    idb_database() = default;

    std::shared_ptr<idb_object_store> create_object_store(const std::string& name,
                                                           const std::string& key_path = "",
                                                           bool auto_increment = false);
    void delete_object_store(const std::string& name);
    std::shared_ptr<idb_transaction> transaction(const std::vector<std::string>& store_names,
                                                  idb_transaction_mode mode = idb_transaction_mode::READ_ONLY);
    void close();

    std::string name() const { return name_; }
    int version() const { return version_; }

private:
    friend class idb_factory;
    std::string name_;
    int version_ = 1;
    std::string db_path_;
    std::map<std::string, std::shared_ptr<idb_object_store>> stores_;
    bool open_ = false;

    void init_schema();
};

class idb_factory {
public:
    static idb_factory& instance();

    std::shared_ptr<idb_request> open(const std::string& name, int version = 1);
    std::shared_ptr<idb_request> delete_database(const std::string& name);
    std::vector<std::string> get_database_names();

    void set_storage_path(const std::string& path);

private:
    idb_factory() = default;
    std::string storage_path_ = "./indexeddb";
    std::map<std::string, std::shared_ptr<idb_database>> databases_;

    std::string db_path(const std::string& name) const;
};

} // namespace hre::storage
