#include <hre/parser/html_tokenizer_bridge.hpp>
#include <cstdlib>
#include <cstring>

// This file provides stub implementations for the Rust HTML tokenizer FFI bridge.
// When the Rust tokenizer is linked, these symbols come from the Rust side.
// These stubs allow compilation without the Rust library.

extern "C" {

HTokenizerHandle* htoken_create(const char* html_utf8) {
    (void)html_utf8;
    return nullptr;
}

int htoken_count(const HTokenizerHandle* handle) {
    (void)handle;
    return 0;
}

int htoken_next(HTokenizerHandle* handle, HToken* out) {
    (void)handle;
    (void)out;
    return 0;
}

void htoken_free(HTokenizerHandle* handle) {
    (void)handle;
}

} // extern "C"
