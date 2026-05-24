cmake_minimum_required(VERSION 3.20)

# Project Definition
project(Hyperion
    VERSION 0.1.0
    DESCRIPTION "Commercial-grade native web browser"
    LANGUAGES CXX C
)

# C++ Standard and MSVC Settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Target Windows 10 RS3+ (1709+) for modern D2D/DWrite interfaces
if(WIN32)
    add_definitions(
        -D_WIN32_WINNT=0x0A00
        -DNTDDI_VERSION=0x0A000004
        -DWINVER=0x0A00
        -DNOMINMAX
        -D_CRT_SECURE_NO_WARNINGS
        -DWIN32_LEAN_AND_MEAN
    )
endif()

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# MSVC Specific Flags
if(MSVC)
    add_compile_options(
        /W4          # Warning level 4
        /permissive- # Standards conformance
        /EHsc        # Standard C++ exception handling
        /MP          # Multi-process compilation
        /utf-8       # Use UTF-8 source/execution character sets
        /Zc:preprocessor # Enable standard preprocessor behavior
        /FS          # Force synchronous PDB writes
    )
    
    # Security flags
    add_compile_options(
        /GS          # Buffer security check
        /sdl         # Additional Security Development Lifecycle checks
        /guard:cf    # Control Flow Guard
    )
    # Optimization for release
    add_compile_options("$<$<CONFIG:Release>:/O2;/Oi;/Ot;/GL>")
    add_link_options("$<$<CONFIG:Release>:/LTCG>")
endif()

# Global Includes
include_directories(HyperionJS/include)

# Subdirectories
add_subdirectory(HyperionJS)
add_subdirectory(platform)
add_subdirectory(ui)
add_subdirectory(engine)
add_subdirectory(browser)
add_subdirectory(apps/hyperion)

# Testing (optional)
option(HYPERION_BUILD_TESTS "Build unit tests" OFF)
if(HYPERION_BUILD_TESTS)
    add_subdirectory(testing)
endif()
