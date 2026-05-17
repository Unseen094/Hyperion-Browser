// ============================================================
// Hyperion HTML Tokenizer — Source Position & Span types
// These are the primitive location types used across ALL modules.
// ============================================================

/// A byte-offset plus line/column for a single source location.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct SourceLocation {
    /// Zero-based byte offset from the start of the source.
    pub offset: usize,
    /// One-based line number.
    pub line: u32,
    /// One-based column number (UTF-8 character count within the line).
    pub column: u32,
}

impl SourceLocation {
    #[inline]
    pub const fn new(offset: usize, line: u32, column: u32) -> Self {
        Self { offset, line, column }
    }
}

impl std::fmt::Display for SourceLocation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.line, self.column)
    }
}

/// A half-open source span [start, end).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct SourceSpan {
    pub start: SourceLocation,
    pub end: SourceLocation,
}

impl SourceSpan {
    #[inline]
    pub const fn new(start: SourceLocation, end: SourceLocation) -> Self {
        Self { start, end }
    }

    /// Byte length of the span.
    #[inline]
    pub const fn byte_len(&self) -> usize {
        self.end.offset.saturating_sub(self.start.offset)
    }
}

impl std::fmt::Display for SourceSpan {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}..{}", self.start, self.end)
    }
}
