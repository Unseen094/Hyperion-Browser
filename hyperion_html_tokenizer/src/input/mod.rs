// ============================================================
// Hyperion HTML Tokenizer — Input Stream
//
// The input stream is the ONLY component that touches raw bytes.
// Everything above this layer works with `char` values and
// `SourceLocation` structs — never raw offsets.
//
// Design goals:
//   - UTF-8 safe (uses Rust's built-in str/char guarantees)
//   - 1-char lookahead via `peek()` / `peek2()`
//   - Full position tracking (offset, line, column)
//   - Future: replace `&str` with a streaming `impl Read` without
//     changing the public API at all.
// ============================================================

use crate::utils::SourceLocation;

/// A zero-allocation, UTF-8-safe, position-tracked character stream.
///
/// The input is borrowed for the lifetime of the stream.
/// No copies of the source are ever made.
pub struct InputStream<'src> {
    /// The raw source being tokenised.
    source: &'src str,
    /// Current byte offset (NOT char offset).
    byte_pos: usize,
    /// Current line (1-based).
    line: u32,
    /// Current column within the line (1-based, char count).
    column: u32,
}

impl<'src> InputStream<'src> {
    /// Construct a new input stream over a UTF-8 source string.
    pub fn new(source: &'src str) -> Self {
        Self {
            source,
            byte_pos: 0,
            line: 1,
            column: 1,
        }
    }

    // ----------------------------------------------------------------
    // Position queries
    // ----------------------------------------------------------------

    /// Returns the current source location (before consuming the next char).
    #[inline]
    pub fn location(&self) -> SourceLocation {
        SourceLocation::new(self.byte_pos, self.line, self.column)
    }

    /// Returns `true` when all input has been consumed.
    #[inline]
    pub fn is_eof(&self) -> bool {
        self.byte_pos >= self.source.len()
    }

    // ----------------------------------------------------------------
    // Lookahead
    // ----------------------------------------------------------------

    /// Peeks at the next character without consuming it.
    /// Returns `None` at EOF.
    #[inline]
    pub fn peek(&self) -> Option<char> {
        self.source[self.byte_pos..].chars().next()
    }

    /// Peeks at the character after next without consuming anything.
    /// Returns `None` if fewer than two characters remain.
    #[inline]
    pub fn peek2(&self) -> Option<char> {
        let mut chars = self.source[self.byte_pos..].chars();
        chars.next()?; // skip first
        chars.next()
    }

    /// Returns the next N chars as a `&str` slice without consuming.
    /// Returns a shorter slice if fewer chars are available.
    pub fn peek_str(&self, n: usize) -> &str {
        let remaining = &self.source[self.byte_pos..];
        // Find byte boundary after n characters
        let end = remaining
            .char_indices()
            .nth(n)
            .map(|(i, _)| i)
            .unwrap_or(remaining.len());
        &remaining[..end]
    }

    // ----------------------------------------------------------------
    // Consumption
    // ----------------------------------------------------------------

    /// Consumes and returns the next character, updating position tracking.
    /// Returns `None` at EOF.
    pub fn next_char(&mut self) -> Option<char> {
        let ch = self.source[self.byte_pos..].chars().next()?;
        self.byte_pos += ch.len_utf8();

        if ch == '\n' {
            self.line += 1;
            self.column = 1;
        } else {
            self.column += 1;
        }

        Some(ch)
    }

    /// Consume the next character only if it equals `expected`.
    /// Returns `true` and advances on a match; does nothing and returns `false` otherwise.
    #[inline]
    pub fn consume_if(&mut self, expected: char) -> bool {
        if self.peek() == Some(expected) {
            self.next_char();
            true
        } else {
            false
        }
    }

    /// Consume characters while `predicate` returns `true`.
    ///
    /// Returns the matched slice (zero-copy borrow of the source).
    pub fn consume_while<F>(&mut self, mut predicate: F) -> &str
    where
        F: FnMut(char) -> bool,
    {
        let start = self.byte_pos;
        while let Some(ch) = self.peek() {
            if !predicate(ch) {
                break;
            }
            self.next_char();
        }
        &self.source[start..self.byte_pos]
    }

    /// Attempt to consume a specific literal string.
    /// If the source does NOT start with `literal`, the stream is unchanged.
    /// Returns `true` on success.
    pub fn consume_str(&mut self, literal: &str) -> bool {
        if self.source[self.byte_pos..].starts_with(literal) {
            for ch in literal.chars() {
                // SAFETY: we already confirmed the bytes exist
                self.byte_pos += ch.len_utf8();
                if ch == '\n' {
                    self.line += 1;
                    self.column = 1;
                } else {
                    self.column += 1;
                }
            }
            true
        } else {
            false
        }
    }

    /// Case-insensitive version of `consume_str`.
    /// Works only on ASCII literals — sufficient for HTML keyword matching.
    pub fn consume_str_ci(&mut self, literal: &str) -> bool {
        let haystack = &self.source[self.byte_pos..];
        if haystack.len() < literal.len() {
            return false;
        }
        let candidate = &haystack[..literal.len()];
        if candidate.eq_ignore_ascii_case(literal) {
            self.consume_str(candidate)
        } else {
            false
        }
    }

    /// Skip any leading ASCII whitespace (space, tab, `\n`, `\r`).
    pub fn skip_whitespace(&mut self) {
        self.consume_while(|c| c.is_ascii_whitespace());
    }

    // ----------------------------------------------------------------
    // Slicing helpers
    // ----------------------------------------------------------------

    /// Return a zero-copy slice of the source from `start_offset` to the current position.
    #[inline]
    pub fn slice_from(&self, start_offset: usize) -> &str {
        &self.source[start_offset..self.byte_pos]
    }
}
