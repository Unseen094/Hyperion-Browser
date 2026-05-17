// ============================================================
// Hyperion CSS Parser — Token Definitions
// ============================================================

use crate::diagnostics::SourceSpan;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CssTokenKind {
    Identifier(String),
    String(String),
    Number(String), // Stored as string to preserve exact format before parsing
    Dimension { value: String, unit: String },
    Percentage(String),
    Hash(String),     // #id or #color
    Function(String), // rgb(, calc(

    Colon,         // :
    Semicolon,     // ;
    Comma,         // ,
    Dot,           // .
    Delim(char),   // >, +, ~, *, etc.

    CurlyOpen,     // {
    CurlyClose,    // }
    ParenOpen,     // (
    ParenClose,    // )
    BracketOpen,   // [
    BracketClose,  // ]

    Whitespace,
    Comment(String),
    Eof,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CssToken {
    pub kind: CssTokenKind,
    pub span: SourceSpan,
}

impl CssToken {
    pub fn new(kind: CssTokenKind, span: SourceSpan) -> Self {
        Self { kind, span }
    }
}
