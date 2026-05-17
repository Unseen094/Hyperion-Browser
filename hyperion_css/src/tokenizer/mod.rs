// ============================================================
// Hyperion CSS Parser — Tokenizer
// ============================================================

pub mod tokens;

use crate::diagnostics::{DiagnosticBag, SourceLocation, SourceSpan};
pub use tokens::{CssToken, CssTokenKind};

pub struct CssTokenizer<'a> {
    source: &'a str,
    chars: std::str::Chars<'a>,
    current_char: Option<char>,
    
    line: usize,
    column: usize,
    byte_offset: usize,

    pub diagnostics: DiagnosticBag,
}

impl<'a> CssTokenizer<'a> {
    pub fn new(source: &'a str) -> Self {
        let mut tokenizer = Self {
            source,
            chars: source.chars(),
            current_char: None,
            line: 1,
            column: 1,
            byte_offset: 0,
            diagnostics: DiagnosticBag::new(),
        };
        tokenizer.advance(); // Load first char
        tokenizer
    }

    pub fn tokenize(mut self) -> (Vec<CssToken>, DiagnosticBag) {
        let mut tokens = Vec::new();
        loop {
            let token = self.next_token();
            let is_eof = token.kind == CssTokenKind::Eof;
            tokens.push(token);
            if is_eof {
                break;
            }
        }
        (tokens, self.diagnostics)
    }

    // ---- Location Tracking ----

    fn location(&self) -> SourceLocation {
        SourceLocation::new(self.line, self.column)
    }

    fn advance(&mut self) {
        if let Some(c) = self.current_char {
            self.byte_offset += c.len_utf8();
            if c == '\n' {
                self.line += 1;
                self.column = 1;
            } else {
                self.column += 1;
            }
        }
        self.current_char = self.chars.next();
    }

    fn peek(&self) -> Option<char> {
        self.current_char
    }
    
    fn peek_next(&self) -> Option<char> {
        self.chars.clone().next()
    }

    // ---- Tokenization Logic ----

    fn next_token(&mut self) -> CssToken {
        let start_loc = self.location();

        let Some(c) = self.peek() else {
            return CssToken::new(CssTokenKind::Eof, SourceSpan::new(start_loc, start_loc));
        };

        match c {
            c if c.is_whitespace() => self.consume_whitespace(start_loc),
            '/' if self.peek_next() == Some('*') => self.consume_comment(start_loc),
            ':' => self.single_char(CssTokenKind::Colon, start_loc),
            ';' => self.single_char(CssTokenKind::Semicolon, start_loc),
            ',' => self.single_char(CssTokenKind::Comma, start_loc),
            '.' => {
                if let Some(n) = self.peek_next() {
                    if n.is_ascii_digit() {
                        return self.consume_number(start_loc);
                    }
                }
                self.single_char(CssTokenKind::Dot, start_loc)
            }
            '{' => self.single_char(CssTokenKind::CurlyOpen, start_loc),
            '}' => self.single_char(CssTokenKind::CurlyClose, start_loc),
            '(' => self.single_char(CssTokenKind::ParenOpen, start_loc),
            ')' => self.single_char(CssTokenKind::ParenClose, start_loc),
            '[' => self.single_char(CssTokenKind::BracketOpen, start_loc),
            ']' => self.single_char(CssTokenKind::BracketClose, start_loc),
            '#' => self.consume_hash(start_loc),
            '"' | '\'' => self.consume_string(start_loc),
            c if is_ident_start(c) => self.consume_ident_like(start_loc),
            c if c.is_ascii_digit() || (c == '-' && self.peek_next().map_or(false, |n| n.is_ascii_digit() || n == '.')) => {
                self.consume_number(start_loc)
            }
            _ => {
                self.advance();
                CssToken::new(CssTokenKind::Delim(c), SourceSpan::new(start_loc, self.location()))
            }
        }
    }

    fn single_char(&mut self, kind: CssTokenKind, start_loc: SourceLocation) -> CssToken {
        self.advance();
        CssToken::new(kind, SourceSpan::new(start_loc, self.location()))
    }

    fn consume_whitespace(&mut self, start_loc: SourceLocation) -> CssToken {
        while let Some(c) = self.peek() {
            if c.is_whitespace() {
                self.advance();
            } else {
                break;
            }
        }
        CssToken::new(CssTokenKind::Whitespace, SourceSpan::new(start_loc, self.location()))
    }

    fn consume_comment(&mut self, start_loc: SourceLocation) -> CssToken {
        self.advance(); // '/'
        self.advance(); // '*'
        
        let mut buf = String::new();
        loop {
            match self.peek() {
                Some('*') => {
                    self.advance();
                    if self.peek() == Some('/') {
                        self.advance();
                        break;
                    }
                    buf.push('*');
                }
                Some(c) => {
                    buf.push(c);
                    self.advance();
                }
                None => {
                    self.diagnostics.warn(SourceSpan::new(start_loc, self.location()), "Unterminated CSS comment");
                    break;
                }
            }
        }

        CssToken::new(CssTokenKind::Comment(buf), SourceSpan::new(start_loc, self.location()))
    }

    fn consume_string(&mut self, start_loc: SourceLocation) -> CssToken {
        let quote = self.peek().unwrap();
        self.advance();

        let mut buf = String::new();
        loop {
            match self.peek() {
                Some(c) if c == quote => {
                    self.advance();
                    break;
                }
                Some('\\') => {
                    self.advance();
                    if let Some(esc) = self.peek() {
                        buf.push(esc);
                        self.advance();
                    }
                }
                Some('\n') | Some('\r') => {
                    self.diagnostics.error(SourceSpan::new(start_loc, self.location()), "Unterminated string literal");
                    break;
                }
                Some(c) => {
                    buf.push(c);
                    self.advance();
                }
                None => {
                    self.diagnostics.error(SourceSpan::new(start_loc, self.location()), "EOF in string literal");
                    break;
                }
            }
        }

        CssToken::new(CssTokenKind::String(buf), SourceSpan::new(start_loc, self.location()))
    }

    fn consume_hash(&mut self, start_loc: SourceLocation) -> CssToken {
        self.advance(); // consume '#'
        let mut name = String::new();
        while let Some(c) = self.peek() {
            if is_ident_char(c) {
                name.push(c);
                self.advance();
            } else {
                break;
            }
        }
        CssToken::new(CssTokenKind::Hash(name), SourceSpan::new(start_loc, self.location()))
    }

    fn consume_ident_like(&mut self, start_loc: SourceLocation) -> CssToken {
        let mut name = String::new();
        while let Some(c) = self.peek() {
            if is_ident_char(c) {
                name.push(c);
                self.advance();
            } else {
                break;
            }
        }

        if self.peek() == Some('(') {
            self.advance(); // consume '('
            CssToken::new(CssTokenKind::Function(name), SourceSpan::new(start_loc, self.location()))
        } else {
            CssToken::new(CssTokenKind::Identifier(name), SourceSpan::new(start_loc, self.location()))
        }
    }

    fn consume_number(&mut self, start_loc: SourceLocation) -> CssToken {
        let mut num = String::new();
        
        if self.peek() == Some('-') {
            num.push('-');
            self.advance();
        }

        while let Some(c) = self.peek() {
            if c.is_ascii_digit() {
                num.push(c);
                self.advance();
            } else {
                break;
            }
        }

        if self.peek() == Some('.') && self.peek_next().map_or(false, |n| n.is_ascii_digit()) {
            num.push('.');
            self.advance();
            while let Some(c) = self.peek() {
                if c.is_ascii_digit() {
                    num.push(c);
                    self.advance();
                } else {
                    break;
                }
            }
        }

        // Check if it's a dimension or percentage
        if self.peek() == Some('%') {
            self.advance();
            CssToken::new(CssTokenKind::Percentage(num), SourceSpan::new(start_loc, self.location()))
        } else if let Some(c) = self.peek() {
            if is_ident_start(c) {
                let mut unit = String::new();
                while let Some(u) = self.peek() {
                    if is_ident_char(u) {
                        unit.push(u);
                        self.advance();
                    } else {
                        break;
                    }
                }
                return CssToken::new(CssTokenKind::Dimension { value: num, unit }, SourceSpan::new(start_loc, self.location()));
            }
            CssToken::new(CssTokenKind::Number(num), SourceSpan::new(start_loc, self.location()))
        } else {
            CssToken::new(CssTokenKind::Number(num), SourceSpan::new(start_loc, self.location()))
        }
    }
}

// ---- Spec Helpers ----

fn is_ident_start(c: char) -> bool {
    c.is_ascii_alphabetic() || c == '_' || c == '-' || c > '\x7F'
}

fn is_ident_char(c: char) -> bool {
    is_ident_start(c) || c.is_ascii_digit()
}
