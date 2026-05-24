cmake_minimum_required(VERSION 3.25)
project(HyperionJS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_library(hjs STATIC
    src/lexer/lexer.cpp
    src/lexer/token.cpp
    src/parser/parser.cpp
    src/parser/ast.cpp
    src/vm/compiler.cpp
    src/vm/vm.cpp
    src/vm/bytecode.cpp
    src/vm/hidden_class.cpp
    src/vm/inline_cache.cpp
    src/vm/jit_baseline.cpp
    src/runtime/stdlib.cpp
    src/runtime/bridge.cpp
    src/runtime/extensions.cpp
    src/runtime/environment.cpp
    src/runtime/object.cpp
    src/runtime/promise.cpp
    src/runtime/array_buffer.cpp
    src/runtime/regexp.cpp
    src/wasm/wasm_module.cpp
    src/wasm/wasm_multi_value.cpp
    src/wasm/wasm_table.cpp
    src/gc/gc.cpp
    src/debug/profiler.cpp
    src/core/value.cpp
)

target_include_directories(hjs PUBLIC include)
