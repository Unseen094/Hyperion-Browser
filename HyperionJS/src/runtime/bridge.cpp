#include <hjs/runtime/object.hpp>
#include <hjs/runtime/environment.hpp>
#include <hjs/core/value.hpp>
#include <hjs/vm/vm.hpp>
#include <iostream>

namespace hjs::runtime {

// Native function: document.setTitle(title)
Value document_setTitle(Value receiver, int arg_count, Value* args) {
    std::wcout << L"[HRE Bridge] Native setTitle called with " << arg_count << L" args" << std::endl;
    if (arg_count > 0 && args[0].is_string()) {
        std::wstring title = args[0].as_string();
        std::wcout << L"[HRE Bridge] Setting Page Title: " << title << std::endl;
    } else {
        std::wcout << L"[HRE Bridge] setTitle Error: No string argument provided" << std::endl;
    }
    return Value();
}

void setup_browser_bindings() {
    // 1. Create 'document' object
    auto document = std::make_shared<JSObject>();
    
    // 2. Add methods to 'document'
    document->set_property(L"setTitle", Value(std::make_shared<NativeFunction>(document_setTitle)));
    
    // 3. Register 'document' globally
    hjs::vm::VM::m_globals.define(L"document", Value(document));
    
    std::wcout << L"[HRE Bridge] Browser bindings initialized." << std::endl;
}

} // namespace hjs::runtime
