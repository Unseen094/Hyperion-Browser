// ============================================================
// Hyperion HTML Tokenizer — Library root
// ============================================================

pub mod diagnostics;
pub mod ffi;
pub mod input;
pub mod states;
pub mod tokens;
pub mod tokenizer;
pub mod utils;
pub mod tests;

pub use tokenizer::Tokenizer;
pub use tokens::Token;
pub use diagnostics::DiagnosticBag;

/// Convenience: tokenize a complete HTML string and return tokens + diagnostics.
pub fn tokenize(source: &str) -> (Vec<Token>, DiagnosticBag) {
    Tokenizer::new(source).tokenize()
}
