#pragma once

#include <hjs/core/value.hpp>

namespace hre::script {

// indexedDB global
hjs::Value native_indexeddb_open(hjs::Value receiver, int argc, hjs::Value* args);

// Database methods
hjs::Value native_idb_database_transaction(hjs::Value receiver, int argc, hjs::Value* args);

// Transaction methods
hjs::Value native_idb_transaction_objectStore(hjs::Value receiver, int argc, hjs::Value* args);

// ObjectStore methods
hjs::Value native_idb_objectStore_add(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_idb_objectStore_get(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_idb_objectStore_put(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_idb_objectStore_delete(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_idb_objectStore_clear(hjs::Value receiver, int argc, hjs::Value* args);

} // namespace hre::script
