#pragma once

#include <hjs/core/value.hpp>

namespace hre::script {

hjs::Value native_websocket_constructor(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_websocket_connect(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_websocket_send(hjs::Value receiver, int argc, hjs::Value* args);
hjs::Value native_websocket_close(hjs::Value receiver, int argc, hjs::Value* args);

} // namespace hre::script
