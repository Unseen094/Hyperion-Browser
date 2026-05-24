#pragma once

#include <hjs/core/value.hpp>

namespace hre::script {

hjs::Value native_xhr_open(hjs::Value receiver, int argc, hjs::Value* args);
 hjs::Value native_xhr_send(hjs::Value receiver, int argc, hjs::Value* args);
 hjs::Value native_xhr_constructor(hjs::Value receiver, int argc, hjs::Value* args);

} // namespace hre::script
