// ============================================================
// Hyperion HTML Tokenizer — Token Definitions
//
// All token variants are defined here. Every token carries:
//   - its structured payload (name, attributes, text, etc.)
//   - a SourceSpan so that later pipeline stages can map tokens
//     back to the original source for error reporting, source maps,
//     devtools, etc.
//
// Rules:
//   - NO parser logic here.
//   - NO DOM coupling.
//   - Tokens are plain data.
// ============================================================

use crate::utils::SourceSpan;

// ---- Attribute ------------------------------------------------------------------

/// A single key=value attribute on a start tag.
///
/// Both key and value are stored as owned strings after
/// entity-decoding (entity support will be added incrementally).
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Attribute {
    /// The attribute name, lower-cased per the HTML5 spec.
    pub name: String,
    /// The attribute value. Empty string when the attribute is bare (e.g. `disabled`).
    pub value: String,
    /// Source span covering the entire `name="value"` run.
    pub span: SourceSpan,
}

impl Attribute {
    pub fn new(name: impl Into<String>, value: impl Into<String>, span: SourceSpan) -> Self {
        Self { name: name.into(), value: value.into(), span }
    }

    /// Returns `true` for boolean attributes (`<input disabled>`).
    pub fn is_boolean(&self) -> bool {
        self.value.is_empty()
    }
}

// ---- DOCTYPE payload ------------------------------------------------------------

/// Payload carried by a DOCTYPE token.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct DoctypeData {
    /// The name following `<!DOCTYPE`, typically "html".
    pub name: Option<String>,
    /// The PUBLIC identifier, if present.
    pub public_id: Option<String>,
    /// The SYSTEM identifier, if present.
    pub system_id: Option<String>,
    /// Whether the parser should treat the document in quirks mode.
    /// Set when the DOCTYPE is missing or malformed.
    pub force_quirks: bool,
}

// ---- Token variants -------------------------------------------------------------

/// Every token variant the tokenizer can produce.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Token {
    /// `<!DOCTYPE …>`
    Doctype {
        data: DoctypeData,
        span: SourceSpan,
    },

    /// `<tag attr="value" …>`
    StartTag {
        /// Tag name, lower-cased.
        name: String,
        /// Parsed attributes in document order.
        attributes: Vec<Attribute>,
        /// `true` if the tag ended with `/>`.
        self_closing: bool,
        span: SourceSpan,
    },

    /// `</tag>`
    EndTag {
        /// Tag name, lower-cased.
        name: String,
        span: SourceSpan,
    },

    /// A run of character data between tags.
    Text {
        /// The decoded text content.
        data: String,
        /// `true` if the data is entirely ASCII whitespace.
        is_whitespace_only: bool,
        span: SourceSpan,
    },

    /// `<!-- … -->`
    Comment {
        data: String,
        span: SourceSpan,
    },

    /// Signals end-of-input to the parser.
    Eof {
        span: SourceSpan,
    },
}

impl Token {
    /// Returns the `SourceSpan` for any token variant.
    pub fn span(&self) -> SourceSpan {
        match self {
            Token::Doctype { span, .. }
            | Token::StartTag { span, .. }
            | Token::EndTag { span, .. }
            | Token::Text { span, .. }
            | Token::Comment { span, .. }
            | Token::Eof { span } => *span,
        }
    }

    /// Returns a human-readable name for the variant (useful in diagnostics / tests).
    pub fn kind_name(&self) -> &'static str {
        match self {
            Token::Doctype { .. } => "Doctype",
            Token::StartTag { .. } => "StartTag",
            Token::EndTag { .. } => "EndTag",
            Token::Text { .. } => "Text",
            Token::Comment { .. } => "Comment",
            Token::Eof { .. } => "Eof",
        }
    }

    /// Convenience: is this the EOF sentinel?
    pub fn is_eof(&self) -> bool {
        matches!(self, Token::Eof { .. })
    }
}

impl std::fmt::Display for Token {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Token::Doctype { data, .. } => {
                write!(f, "Doctype({})", data.name.as_deref().unwrap_or("?"))
            }
            Token::StartTag { name, attributes, self_closing, .. } => {
                write!(f, "StartTag({}", name)?;
                for attr in attributes {
                    write!(f, " {}={:?}", attr.name, attr.value)?;
                }
                if *self_closing {
                    write!(f, " /")?;
                }
                write!(f, ")")
            }
            Token::EndTag { name, .. } => write!(f, "EndTag({name})"),
            Token::Text { data, .. } => write!(f, "Text({:?})", data),
            Token::Comment { data, .. } => write!(f, "Comment({:?})", data),
            Token::Eof { .. } => write!(f, "Eof"),
        }
    }
}
