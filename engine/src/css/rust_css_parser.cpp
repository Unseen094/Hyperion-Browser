#include "hre/css/css_parser.hpp"

// Link to Rust library
#ifdef _WIN32
    #pragma comment(lib, "hyperion_css_parser.lib")
#endif

// The FFI functions are declared in css_parser.hpp as extern "C"
// This file mainly serves to link against the Rust library

namespace hre::css {
    // Implementation is in the header and css_parser.cpp
}
