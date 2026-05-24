#pragma once

#include <hjs/core/value.hpp>

namespace hre::script {

hjs::Value native_fetch(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_local_storage_get(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_local_storage_set(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_local_storage_remove(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_local_storage_clear(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_session_storage_get(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_session_storage_set(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_session_storage_remove(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_session_storage_clear(hjs::Value receiver, int argc, hjs::Value* args);

} // namespace hre::script