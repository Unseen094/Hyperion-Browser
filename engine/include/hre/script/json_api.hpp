#pragma once

#include <hjs/core/value.hpp>

namespace hre::script {

hjs::Value native_json_parse(hjs::Value, int argc, hjs::Value* args);
hjs::Value native_json_stringify(hjs::Value, int argc, hjs::Value* args);

} // namespace hre::script
