#pragma once

#include <hjs/core/value.hpp>
#include <hjs/runtime/object.hpp>
#include <memory>

namespace hjs::runtime {

class StdLib {
public:
    static void initialize();

    static std::shared_ptr<JSObject> array_prototype;
    static std::shared_ptr<JSObject> string_prototype;
    static std::shared_ptr<JSObject> math_object;
};

} // namespace hjs::runtime
