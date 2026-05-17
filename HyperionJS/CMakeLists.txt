cmake_minimum_required(VERSION 3.25)
project(HyperionJS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_library(hjs STATIC
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/vm/compiler.cpp
    src/vm/vm.cpp
    src/runtime/stdlib.cpp
)

add_executable(hjs_cli src/main.cpp)
target_link_libraries(hjs_cli hjs)
