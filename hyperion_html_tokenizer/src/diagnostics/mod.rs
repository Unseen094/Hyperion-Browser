// ============================================================
// Hyperion HTML Tokenizer — Diagnostics
//
// All tokenizer warnings and recoverable errors are emitted
// through this system. The tokenizer NEVER panics or crashes on
// malformed HTML; instead it records a diagnostic and resumes.
// ============================================================

use crate::utils::{SourceLocation, SourceSpan};

/// The severity of a diagnostic.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Severity {
    /// A deviation from the spec that is safely recovered from.
    Warning,
    /// An error in structure that the tokenizer has recovered from.
    Error,
}

impl std::fmt::Display for Severity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Severity::Warning => write!(f, "warning"),
            Severity::Error => write!(f, "error"),
        }
    }
}

/// A single diagnostic message with source location.
#[derive(Debug, Clone)]
pub struct Diagnostic {
    pub severity: Severity,
    pub span: SourceSpan,
    pub code: DiagnosticCode,
    pub message: String,
}

impl Diagnostic {
    pub fn warning(span: SourceSpan, code: DiagnosticCode, message: impl Into<String>) -> Self {
        Self { severity: Severity::Warning, span, code, message: message.into() }
    }

    pub fn error(span: SourceSpan, code: DiagnosticCode, message: impl Into<String>) -> Self {
        Self { severity: Severity::Error, span, code, message: message.into() }
    }
}

impl std::fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "[{}] {} ({}): {}", self.severity, self.code, self.span, self.message)
    }
}

/// Stable, machine-readable diagnostic codes.
///
/// Adding a new code is the ONLY way to introduce a new diagnostic.
/// This keeps the diagnostic surface structured and avoids arbitrary string comparisons.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u16)]
pub enum DiagnosticCode {
    // --- Input errors ---
    /// A null byte (\0) was encountered in the input stream.
    NullCharacterInInput = 1001,
    /// A non-Unicode code point was detected.
    InvalidUtf8Sequence = 1002,

    // --- Tag errors ---
    /// A '<' was not followed by a valid tag opener character.
    UnexpectedCharAfterLt = 2001,
    /// A tag was opened but never closed before EOF.
    UnterminatedTag = 2002,
    /// An end-tag contained attributes (not permitted by spec).
    AttributesInEndTag = 2003,
    /// Two consecutive `=` in an attribute assignment.
    MalformedAttributeAssignment = 2004,
    /// A start tag ends in `/` but is not a void element (parsed as self-closing anyway).
    SelfClosingNonVoidElement = 2005,
    /// A tag name started with a digit or invalid character.
    InvalidTagNameStart = 2006,

    // --- Comment errors ---
    /// A comment was opened (`<!--`) but never terminated before EOF.
    UnterminatedComment = 3001,
    /// `--` appeared inside a comment body (spec-non-conformant but recoverable).
    DoubleHyphenInComment = 3002,

    // --- DOCTYPE errors ---
    /// DOCTYPE keyword was malformed.
    MalformedDoctype = 4001,

    // --- General ---
    /// EOF was reached in an unexpected tokenizer state.
    UnexpectedEof = 9001,
}

impl std::fmt::Display for DiagnosticCode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "HT{:04}", *self as u16)
    }
}

/// Accumulates diagnostics during a tokenization pass.
///
/// Callers may inspect the list after tokenization to surface
/// warnings/errors in a developer console or build pipeline.
#[derive(Debug, Default)]
pub struct DiagnosticBag {
    entries: Vec<Diagnostic>,
}

impl DiagnosticBag {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, d: Diagnostic) {
        self.entries.push(d);
    }

    pub fn warn(
        &mut self,
        start: SourceLocation,
        end: SourceLocation,
        code: DiagnosticCode,
        msg: impl Into<String>,
    ) {
        self.entries.push(Diagnostic::warning(SourceSpan::new(start, end), code, msg));
    }

    pub fn error(
        &mut self,
        start: SourceLocation,
        end: SourceLocation,
        code: DiagnosticCode,
        msg: impl Into<String>,
    ) {
        self.entries.push(Diagnostic::error(SourceSpan::new(start, end), code, msg));
    }

    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn has_errors(&self) -> bool {
        self.entries.iter().any(|d| d.severity >= Severity::Error)
    }

    pub fn iter(&self) -> impl Iterator<Item = &Diagnostic> {
        self.entries.iter()
    }
}
