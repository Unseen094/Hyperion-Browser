#pragma once
// ============================================================
// Hyperion HTML Tokenizer — C++ Bridge Header
//
// This header is the ONLY interface C++ code uses to talk to the
// Rust HTML tokenizer. It mirrors the Rust FFI structs exactly.
//
// Link requirements (handled by CMake):
//   hyperion_html_tokenizer.lib  (Rust release build)
//   ws2_32.lib, userenv.lib, ntdll.lib  (Rust runtime deps on Windows)
// ============================================================

#include <cstdint>

extern "C" {

// ---- Constants (must match Rust ffi/mod.rs) ----
static constexpr int HTOKEN_NAME_BUF  = 256;
static constexpr int HTOKEN_DATA_BUF  = 4096;
static constexpr int HTOKEN_MAX_ATTRS = 32;

// ---- Token kind ----
enum class HTokenKind : int {
    Doctype  = 0,
    StartTag = 1,
    EndTag   = 2,
    Text     = 3,
    Comment  = 4,
    Eof      = 5,
};

// ---- Attribute ----
struct HAttribute {
    char name [HTOKEN_NAME_BUF];
    char value[HTOKEN_DATA_BUF];
};

// ---- Token ----
struct HToken {
    HTokenKind kind;
    char       name[HTOKEN_NAME_BUF];
    char       data[HTOKEN_DATA_BUF];
    int        self_closing;
    int        force_quirks;
    int        attr_count;
    HAttribute attrs[HTOKEN_MAX_ATTRS];
    char       public_id[HTOKEN_NAME_BUF];
    char       system_id[HTOKEN_NAME_BUF];
    int        line;
    int        column;
};

// ---- Opaque handle ----
struct HTokenizerHandle;

// ---- API ----

/// Create tokenizer from a null-terminated UTF-8 HTML string.
/// Returns nullptr on invalid input.
HTokenizerHandle* htoken_create(const char* html_utf8);

/// Total token count (including EOF sentinel).
int htoken_count(const HTokenizerHandle* handle);

/// Advance and fill `out`. Returns 1 while tokens remain, 0 at EOF.
int htoken_next(HTokenizerHandle* handle, HToken* out);

/// Free the handle. Must be called exactly once per htoken_create call.
void htoken_free(HTokenizerHandle* handle);

} // extern "C"
