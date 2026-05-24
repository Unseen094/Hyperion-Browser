// ============================================================
// Hyperion HTML Tokenizer — C FFI Bridge
//
// This module exposes a flat, allocation-free C ABI that C++ can
// call directly after linking against the Rust staticlib.
//
// Design rules:
//  - No heap allocation crosses the FFI boundary.
//  - All strings are UTF-8 (C++ side converts to wstring as needed).
//  - Caller owns nothing; all memory is owned by the TokenizerHandle.
//  - Every function is marked `#[no_mangle] extern "C"`.
// ============================================================

use crate::tokenizer::Tokenizer;
use crate::tokens::Token;
use std::os::raw::{c_char, c_int};

// ---- C-compatible token kind ----------------------------------------

/// Mirror of the Rust Token variants — safe to pass across FFI.
#[repr(C)]
pub enum CTokenKind {
    Doctype = 0,
    StartTag = 1,
    EndTag = 2,
    Text = 3,
    Comment = 4,
    Eof = 5,
}

/// Maximum tag/attribute name length (bytes).
const NAME_BUF: usize = 256;
/// Maximum text/comment/value payload (bytes, UTF-8).
const DATA_BUF: usize = 4096;
/// Maximum attributes per token.
const MAX_ATTRS: usize = 32;

/// A single attribute in C layout.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CAttribute {
    pub name: [c_char; NAME_BUF],
    pub value: [c_char; DATA_BUF],
}

/// A single token in C layout — all fields are inline buffers, no pointers.
#[repr(C)]
pub struct CToken {
    pub kind: CTokenKind,
    /// Tag/DOCTYPE name, or empty for Text/Comment.
    pub name: [c_char; NAME_BUF],
    /// Text data, comment body, or DOCTYPE name fallback.
    pub data: [c_char; DATA_BUF],
    pub self_closing: c_int,
    pub force_quirks: c_int,
    pub attr_count: c_int,
    pub attrs: [CAttribute; MAX_ATTRS],
    /// DOCTYPE public identifier
    pub public_id: [c_char; NAME_BUF],
    /// DOCTYPE system identifier
    pub system_id: [c_char; NAME_BUF],
    /// Source line (1-based).
    pub line: c_int,
    /// Source column (1-based).
    pub column: c_int,
}

// ---- Handle ---------------------------------------------------------

/// Opaque handle that owns the pre-computed token list.
pub struct TokenizerHandle {
    tokens: Vec<Token>,
    cursor: usize,
}

// ---- Helpers --------------------------------------------------------

fn write_str(buf: &mut [c_char], s: &str) {
    let bytes = s.as_bytes();
    let len = bytes.len().min(buf.len() - 1);
    for (i, &b) in bytes[..len].iter().enumerate() {
        buf[i] = b as c_char;
    }
    buf[len] = 0;
}

fn blank_token(kind: CTokenKind) -> CToken {
    CToken {
        kind,
        name: [0; NAME_BUF],
        data: [0; DATA_BUF],
        self_closing: 0,
        force_quirks: 0,
        attr_count: 0,
        attrs: [CAttribute { name: [0; NAME_BUF], value: [0; DATA_BUF] }; MAX_ATTRS],
        public_id: [0; NAME_BUF],
        system_id: [0; NAME_BUF],
        line: 0,
        column: 0,
    }
}

fn rust_token_to_c(tok: &Token) -> CToken {
    match tok {
        Token::Doctype { data, span } => {
            let mut ct = blank_token(CTokenKind::Doctype);
            if let Some(name) = &data.name {
                write_str(&mut ct.name, name);
                write_str(&mut ct.data, name);
            }
            if let Some(pid) = &data.public_id {
                write_str(&mut ct.public_id, pid);
            }
            if let Some(sid) = &data.system_id {
                write_str(&mut ct.system_id, sid);
            }
            ct.force_quirks = data.force_quirks as c_int;
            ct.line = span.start.line as c_int;
            ct.column = span.start.column as c_int;
            ct
        }
        Token::StartTag { name, attributes, self_closing, span } => {
            let mut ct = blank_token(CTokenKind::StartTag);
            write_str(&mut ct.name, name);
            ct.self_closing = *self_closing as c_int;
            let count = attributes.len().min(MAX_ATTRS);
            ct.attr_count = count as c_int;
            for (i, attr) in attributes[..count].iter().enumerate() {
                write_str(&mut ct.attrs[i].name, &attr.name);
                write_str(&mut ct.attrs[i].value, &attr.value);
            }
            ct.line = span.start.line as c_int;
            ct.column = span.start.column as c_int;
            ct
        }
        Token::EndTag { name, span } => {
            let mut ct = blank_token(CTokenKind::EndTag);
            write_str(&mut ct.name, name);
            ct.line = span.start.line as c_int;
            ct.column = span.start.column as c_int;
            ct
        }
        Token::Text { data, span, .. } => {
            let mut ct = blank_token(CTokenKind::Text);
            write_str(&mut ct.data, data);
            ct.line = span.start.line as c_int;
            ct.column = span.start.column as c_int;
            ct
        }
        Token::Comment { data, span } => {
            let mut ct = blank_token(CTokenKind::Comment);
            write_str(&mut ct.data, data);
            ct.line = span.start.line as c_int;
            ct.column = span.start.column as c_int;
            ct
        }
        Token::Eof { span } => {
            let mut ct = blank_token(CTokenKind::Eof);
            ct.line = span.start.line as c_int;
            ct.column = span.start.column as c_int;
            ct
        }
    }
}

// ---- Public FFI exports ---------------------------------------------

/// Create a tokenizer handle from a null-terminated UTF-8 HTML string.
/// Returns an opaque pointer; must be freed with `htoken_free`.
#[no_mangle]
pub extern "C" fn htoken_create(html_utf8: *const c_char) -> *mut TokenizerHandle {
    if html_utf8.is_null() {
        return std::ptr::null_mut();
    }
    let cstr = unsafe { std::ffi::CStr::from_ptr(html_utf8) };
    let source = match cstr.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let (tokens, _diags) = Tokenizer::new(source).tokenize();
    let handle = Box::new(TokenizerHandle { tokens, cursor: 0 });
    Box::into_raw(handle)
}

/// Returns the total number of tokens (including EOF).
#[no_mangle]
pub extern "C" fn htoken_count(handle: *const TokenizerHandle) -> c_int {
    if handle.is_null() { return 0; }
    unsafe { (*handle).tokens.len() as c_int }
}

/// Fill `out` with the next token. Returns 1 on success, 0 at EOF / error.
#[no_mangle]
pub extern "C" fn htoken_next(handle: *mut TokenizerHandle, out: *mut CToken) -> c_int {
    if handle.is_null() || out.is_null() { return 0; }
    let h = unsafe { &mut *handle };
    if h.cursor >= h.tokens.len() { return 0; }
    let tok = &h.tokens[h.cursor];
    h.cursor += 1;
    unsafe { *out = rust_token_to_c(tok); }
    if matches!(tok, Token::Eof { .. }) { 0 } else { 1 }
}

/// Free the tokenizer handle and all its memory.
#[no_mangle]
pub extern "C" fn htoken_free(handle: *mut TokenizerHandle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)); }
    }
}
