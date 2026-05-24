#include "hre/script/indexeddb_api.hpp"
#include "hre/storage/indexeddb.hpp"
#include <hjs/runtime/promise.hpp>
#include <hjs/runtime/object.hpp>

namespace hre::script {

// Helper: get or create store, execute an operation
static hre::storage::idb_object_store* resolve_store(const std::wstring& dbName, const std::wstring& storeName) {
    auto& engine = hre::storage::indexeddb_engine::instance();
    auto db = engine.open(dbName);
    if (!db) return nullptr;
    auto* store = db->get_object_store(storeName);
    if (!store) {
        store = db->create_object_store(storeName);
    }
    return store;
}

// Helper to retrieve hidden string property safely
static std::wstring get_hidden_string(hjs::JSObject* obj, const std::wstring& name) {
    if (!obj) return L"";
    hjs::Value* v = obj->get_property(name);
    if (v && v->is_string()) return v->as_string();
    return L"";
}

// ----- indexedDB global -----

hjs::Value native_indexeddb_open(hjs::Value, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) {
        auto p = std::make_shared<hjs::Promise>();
        p->reject(hjs::Value(L"Invalid arguments to indexedDB.open"));
        return hjs::Value(p);
    }
    std::wstring dbName = args[0].as_string();
    auto dbObj = std::make_shared<hjs::JSObject>();
    dbObj->set_property(L"_name", hjs::Value(dbName));
    dbObj->set_property(L"transaction", hjs::Value(std::make_shared<hjs::NativeFunction>(native_idb_database_transaction)));
    auto promise = std::make_shared<hjs::Promise>();
    promise->resolve(hjs::Value(dbObj));
    return hjs::Value(promise);
}

// ----- Database -----

hjs::Value native_idb_database_transaction(hjs::Value receiver, int argc, hjs::Value* args) {
    auto txnObj = std::make_shared<hjs::JSObject>();
    if (auto* dbObj = dynamic_cast<hjs::JSObject*>(receiver.as_object().get())) {
        hjs::Value* nameVal = dbObj->get_property(L"_name");
        if (nameVal && nameVal->is_string()) {
            txnObj->set_property(L"_dbName", *nameVal);
        }
    }
    txnObj->set_property(L"objectStore", hjs::Value(std::make_shared<hjs::NativeFunction>(native_idb_transaction_objectStore)));
    return hjs::Value(txnObj);
}

// ----- Transaction -----

hjs::Value native_idb_transaction_objectStore(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 1 || !args[0].is_string()) return hjs::Value();
    std::wstring storeName = args[0].as_string();
    auto storeObj = std::make_shared<hjs::JSObject>();
    if (auto* txnObj = dynamic_cast<hjs::JSObject*>(receiver.as_object().get())) {
        hjs::Value* dbNameVal = txnObj->get_property(L"_dbName");
        if (dbNameVal && dbNameVal->is_string()) {
            storeObj->set_property(L"_dbName", *dbNameVal);
        }
    }
    storeObj->set_property(L"_storeName", hjs::Value(storeName));
    storeObj->set_property(L"add", hjs::Value(std::make_shared<hjs::NativeFunction>(native_idb_objectStore_add)));
    storeObj->set_property(L"get", hjs::Value(std::make_shared<hjs::NativeFunction>(native_idb_objectStore_get)));
    storeObj->set_property(L"put", hjs::Value(std::make_shared<hjs::NativeFunction>(native_idb_objectStore_put)));
    storeObj->set_property(L"delete", hjs::Value(std::make_shared<hjs::NativeFunction>(native_idb_objectStore_delete)));
    storeObj->set_property(L"clear", hjs::Value(std::make_shared<hjs::NativeFunction>(native_idb_objectStore_clear)));
    return hjs::Value(storeObj);
}

// ----- ObjectStore -----

hjs::Value native_idb_objectStore_add(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 2) return hjs::Value();
    std::wstring value = args[0].is_string() ? args[0].as_string() : args[0].to_string();
    std::wstring key = args[1].is_string() ? args[1].as_string() : args[1].to_string();
    std::wstring dbName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_dbName");
    std::wstring storeName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_storeName");
    if (dbName.empty() || storeName.empty()) return hjs::Value();
    auto* store = resolve_store(dbName, storeName);
    if (store) store->put(key, value);
    auto p = std::make_shared<hjs::Promise>();
    p->resolve(hjs::Value());
    return hjs::Value(p);
}

hjs::Value native_idb_objectStore_get(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 1) return hjs::Value();
    std::wstring key = args[0].is_string() ? args[0].as_string() : args[0].to_string();
    std::wstring dbName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_dbName");
    std::wstring storeName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_storeName");
    std::wstring out;
    bool found = false;
    auto* store = resolve_store(dbName, storeName);
    if (store) found = store->get(key, out);
    auto p = std::make_shared<hjs::Promise>();
    if (found) p->resolve(hjs::Value(out));
    else p->resolve(hjs::Value());
    return hjs::Value(p);
}

hjs::Value native_idb_objectStore_put(hjs::Value receiver, int argc, hjs::Value* args) {
    return native_idb_objectStore_add(receiver, argc, args);
}

hjs::Value native_idb_objectStore_delete(hjs::Value receiver, int argc, hjs::Value* args) {
    if (argc < 1) return hjs::Value();
    std::wstring key = args[0].is_string() ? args[0].as_string() : args[0].to_string();
    std::wstring dbName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_dbName");
    std::wstring storeName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_storeName");
    auto* store = resolve_store(dbName, storeName);
    if (store) store->remove(key);
    auto p = std::make_shared<hjs::Promise>();
    p->resolve(hjs::Value());
    return hjs::Value(p);
}

hjs::Value native_idb_objectStore_clear(hjs::Value receiver, int, hjs::Value*) {
    std::wstring dbName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_dbName");
    std::wstring storeName = get_hidden_string(dynamic_cast<hjs::JSObject*>(receiver.as_object().get()), L"_storeName");
    auto* store = resolve_store(dbName, storeName);
    if (store) store->clear();
    auto p = std::make_shared<hjs::Promise>();
    p->resolve(hjs::Value());
    return hjs::Value(p);
}

} // namespace hre::script