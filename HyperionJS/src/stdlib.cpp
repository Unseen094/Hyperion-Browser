#include <HyperionJS/include/hyperion_js/vm.hpp>
#include <HyperionJS/include/hyperion_js/value.hpp>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace hyperion::js {

void vm::init_standard_library() {
    // 1. Initialize 'Math' Object
    auto math_obj = create_object();
    set_native_prop(math_obj, "PI", value::number(3.141592653589793));
    set_native_func(math_obj, "abs", [](const std::vector<value>& args) {
        return args.empty() ? value::number(0) : value::number(std::abs(args[0].as_number()));
    });
    set_native_func(math_obj, "sqrt", [](const std::vector<value>& args) {
        return args.empty() ? value::number(0) : value::number(std::sqrt(args[0].as_number()));
    });
    set_native_func(math_obj, "floor", [](const std::vector<value>& args) {
        return args.empty() ? value::number(0) : value::number(std::floor(args[0].as_number()));
    });
    m_global_object.properties["Math"] = math_obj;

    // 2. Initialize 'console' Object
    auto console_obj = create_object();
    set_native_func(console_obj, "log", [](const std::vector<value>& args) {
        for (const auto& arg : args) std::cout << arg.to_string() << " ";
        std::cout << std::endl;
        return value::undefined();
    });
    m_global_object.properties["console"] = console_obj;

    // 3. Initialize Array Prototype
    m_array_prototype = create_object();
    set_native_func(m_array_prototype, "push", [this](const std::vector<value>& args) {
        // Implementation of Array.push logic
        return value::number(0); 
    });
}

value vm::create_object() {
    auto* obj = m_gc.allocate<object>();
    return value::object_ptr(obj);
}

void vm::set_native_prop(value obj, const std::string& name, value val) {
    if (obj.is_object()) {
        obj.as_object()->properties[name] = val;
    }
}

void vm::set_native_func(value obj, const std::string& name, native_function func) {
    if (obj.is_object()) {
        auto* native_fn = m_gc.allocate<function_object>();
        native_fn->is_native = true;
        native_fn->native_code = func;
        obj.as_object()->properties[name] = value::object_ptr(native_fn);
    }
}

} // namespace hyperion::js
