# Testing configuration for Hyperion Engine
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(hre_tests
    ../engine/tests/layout_engine_test.cpp
    ../engine/tests/flexbox_engine_test.cpp
    ../engine/tests/grid_engine_test.cpp
    ../engine/tests/positioning_engine_test.cpp
    ../engine/tests/table_engine_test.cpp
    ../engine/tests/replaced_engine_test.cpp
    ../engine/tests/overflow_engine_test.cpp
)

target_link_libraries(hre_tests
    PRIVATE
    hre
    GTest::gtest_main
)

target_include_directories(hre_tests
    PRIVATE
    ../engine/include
)

# Register tests
include(GoogleTest)
gtest_discover_tests(hre_tests)
