// ============================================================
// Hyperion HTML Tokenizer — Core Tokenizer
// ============================================================

use crate::diagnostics::{DiagnosticBag, DiagnosticCode};
use crate::input::InputStream;
use crate::states::TokenizerState;
use crate::tokens::{Attribute, DoctypeData, Token};
use crate::utils::{SourceLocation, SourceSpan, decode_html_entities};

/// In-progress tag being assembled before emission.
#[derive(Debug, Default)]
struct TagBuilder {
    name: String,
    attributes: Vec<Attribute>,
    self_closing: bool,
    is_end_tag: bool,
    start: SourceLocation,
    // attribute being assembled
    cur_attr_name: String,
    cur_attr_value: String,
    cur_attr_start: SourceLocation,
}

impl TagBuilder {
    fn flush_current_attr(&mut self, end: SourceLocation) {
        if !self.cur_attr_name.is_empty() {
            let decoded_val = decode_html_entities(&self.cur_attr_value);
            self.attributes.push(Attribute::new(
                std::mem::take(&mut self.cur_attr_name),
                decoded_val,
                SourceSpan::new(self.cur_attr_start, end),
            ));
            self.cur_attr_value.clear();
        }
    }
}

/// The HTML5 tokenizer.
pub struct Tokenizer<'src> {
    input: InputStream<'src>,
    state: TokenizerState,
    pub diagnostics: DiagnosticBag,
    // pending items
    tag: TagBuilder,
    text_buf: String,
    text_start: SourceLocation,
    comment_buf: String,
    comment_start: SourceLocation,
    doctype: DoctypeData,
    doctype_start: SourceLocation,
    /// All emitted tokens — caller drains via `tokens()`.
    output: Vec<Token>,
}

impl<'src> Tokenizer<'src> {
    pub fn new(source: &'src str) -> Self {
        Self {
            input: InputStream::new(source),
            state: TokenizerState::Data,
            diagnostics: DiagnosticBag::new(),
            tag: TagBuilder::default(),
            text_buf: String::new(),
            text_start: SourceLocation::default(),
            comment_buf: String::new(),
            comment_start: SourceLocation::default(),
            doctype: DoctypeData::default(),
            doctype_start: SourceLocation::default(),
            output: Vec::with_capacity(64),
        }
    }

    /// Run to completion and return all tokens.
    pub fn tokenize(mut self) -> (Vec<Token>, DiagnosticBag) {
        while self.state != TokenizerState::Eof {
            self.step();
        }
        (self.output, self.diagnostics)
    }

    // -------------------------------------------------------
    // Main dispatch
    // -------------------------------------------------------

    fn step(&mut self) {
        match self.state {
            TokenizerState::Data => self.handle_data(),
            TokenizerState::TagOpen => self.handle_tag_open(),
            TokenizerState::EndTagOpen => self.handle_end_tag_open(),
            TokenizerState::TagName => self.handle_tag_name(),
            TokenizerState::BeforeAttributeName => self.handle_before_attr_name(),
            TokenizerState::AttributeName => self.handle_attr_name(),
            TokenizerState::AfterAttributeName => self.handle_after_attr_name(),
            TokenizerState::BeforeAttributeValue => self.handle_before_attr_value(),
            TokenizerState::AttributeValueDoubleQuoted => self.handle_attr_value_dquote(),
            TokenizerState::AttributeValueSingleQuoted => self.handle_attr_value_squote(),
            TokenizerState::AttributeValueUnquoted => self.handle_attr_value_unquoted(),
            TokenizerState::AfterAttributeValueQuoted => self.handle_after_attr_value_quoted(),
            TokenizerState::SelfClosingStartTag => self.handle_self_closing(),
            TokenizerState::MarkupDeclarationOpen => self.handle_markup_decl_open(),
            TokenizerState::Comment => self.handle_comment(),
            TokenizerState::CommentEndDash => self.handle_comment_end_dash(),
            TokenizerState::CommentEnd => self.handle_comment_end(),
            TokenizerState::Doctype => self.handle_doctype(),
            TokenizerState::DoctypeName => self.handle_doctype_name(),
            TokenizerState::ScriptData => self.handle_script_data(),
            TokenizerState::ScriptDataLessThanSign => self.handle_script_data_lt(),
            TokenizerState::ScriptDataEndTagOpen => self.handle_script_data_end_tag_open(),
            TokenizerState::ScriptDataEndTagName => self.handle_script_data_end_tag_name(),
            TokenizerState::Eof => {}
        }
    }

    // -------------------------------------------------------
    // Helpers
    // -------------------------------------------------------

    fn flush_text(&mut self) {
        if !self.text_buf.is_empty() {
            let mut data = std::mem::take(&mut self.text_buf);
            
            // Only decode entities in normal Data state, not ScriptData
            if matches!(self.state, TokenizerState::Data | TokenizerState::TagOpen | TokenizerState::Eof) {
                data = decode_html_entities(&data);
            }

            let is_ws = data.chars().all(|c| c.is_ascii_whitespace());
            let end = self.input.location();
            self.output.push(Token::Text {
                data,
                is_whitespace_only: is_ws,
                span: SourceSpan::new(self.text_start, end),
            });
        }
    }

    fn emit_eof(&mut self) {
        let loc = self.input.location();
        self.output.push(Token::Eof { span: SourceSpan::new(loc, loc) });
        self.state = TokenizerState::Eof;
    }

    fn emit_tag(&mut self) {
        let end = self.input.location();
        let mut tag = std::mem::take(&mut self.tag);
        tag.flush_current_attr(end);

        let tag_name = tag.name.clone();
        let is_end_tag = tag.is_end_tag;

        if is_end_tag {
            self.output.push(Token::EndTag {
                name: tag.name,
                span: SourceSpan::new(tag.start, end),
            });
        } else {
            self.output.push(Token::StartTag {
                name: tag.name,
                attributes: tag.attributes,
                self_closing: tag.self_closing,
                span: SourceSpan::new(tag.start, end),
            });
        }

        if !is_end_tag && tag_name == "script" {
            self.state = TokenizerState::ScriptData;
        } else {
            self.state = TokenizerState::Data;
        }
    }

    fn emit_comment(&mut self) {
        let end = self.input.location();
        let data = std::mem::take(&mut self.comment_buf);
        self.output.push(Token::Comment {
            data,
            span: SourceSpan::new(self.comment_start, end),
        });
        self.state = TokenizerState::Data;
    }

    fn emit_doctype(&mut self) {
        let end = self.input.location();
        let data = std::mem::take(&mut self.doctype);
        self.output.push(Token::Doctype {
            data,
            span: SourceSpan::new(self.doctype_start, end),
        });
        self.state = TokenizerState::Data;
    }

    fn diag_warn(&mut self, code: DiagnosticCode, msg: &str) {
        let loc = self.input.location();
        self.diagnostics.warn(loc, loc, code, msg);
    }

    fn diag_error(&mut self, code: DiagnosticCode, msg: &str) {
        let loc = self.input.location();
        self.diagnostics.error(loc, loc, code, msg);
    }

    // -------------------------------------------------------
    // State handlers
    // -------------------------------------------------------

    fn handle_data(&mut self) {
        match self.input.peek() {
            None => {
                self.flush_text();
                self.emit_eof();
            }
            Some('<') => {
                self.flush_text();
                self.input.next_char();
                self.state = TokenizerState::TagOpen;
            }
            Some('\0') => {
                self.diag_warn(DiagnosticCode::NullCharacterInInput, "Null character in data");
                self.input.next_char();
                self.text_buf.push('\u{FFFD}');
            }
            Some(ch) => {
                if self.text_buf.is_empty() {
                    self.text_start = self.input.location();
                }
                self.text_buf.push(ch);
                self.input.next_char();
            }
        }
    }

    fn handle_tag_open(&mut self) {
        match self.input.peek() {
            None => {
                // Bare '<' at EOF — emit as text
                self.diag_error(DiagnosticCode::UnexpectedEof, "EOF after '<'");
                self.text_buf.push('<');
                self.flush_text();
                self.emit_eof();
            }
            Some('!') => {
                self.input.next_char();
                self.state = TokenizerState::MarkupDeclarationOpen;
            }
            Some('/') => {
                self.input.next_char();
                self.state = TokenizerState::EndTagOpen;
            }
            Some(c) if c.is_ascii_alphabetic() => {
                self.tag = TagBuilder::default();
                self.tag.start = self.input.location();
                self.state = TokenizerState::TagName;
            }
            Some(c) => {
                self.diag_error(
                    DiagnosticCode::UnexpectedCharAfterLt,
                    &format!("Unexpected '{}' after '<'", c),
                );
                // Recover: treat as text
                self.text_buf.push('<');
                self.state = TokenizerState::Data;
            }
        }
    }

    fn handle_end_tag_open(&mut self) {
        match self.input.peek() {
            None => {
                self.diag_error(DiagnosticCode::UnexpectedEof, "EOF in end tag");
                self.emit_eof();
            }
            Some(c) if c.is_ascii_alphabetic() => {
                self.tag = TagBuilder { is_end_tag: true, start: self.input.location(), ..Default::default() };
                self.state = TokenizerState::TagName;
            }
            Some('>') => {
                // </> — empty end tag, ignore
                self.input.next_char();
                self.state = TokenizerState::Data;
            }
            Some(c) => {
                self.diag_error(
                    DiagnosticCode::InvalidTagNameStart,
                    &format!("Invalid char '{}' in end tag", c),
                );
                self.state = TokenizerState::Data;
            }
        }
    }

    fn handle_tag_name(&mut self) {
        match self.input.peek() {
            None => {
                self.diag_error(DiagnosticCode::UnterminatedTag, "EOF in tag name");
                self.emit_eof();
            }
            Some(c) if c.is_ascii_whitespace() => {
                self.input.next_char();
                self.state = TokenizerState::BeforeAttributeName;
            }
            Some('/') => {
                self.input.next_char();
                self.state = TokenizerState::SelfClosingStartTag;
            }
            Some('>') => {
                self.input.next_char();
                self.emit_tag();
            }
            Some(c) => {
                self.tag.name.push(c.to_ascii_lowercase());
                self.input.next_char();
            }
        }
    }

    fn handle_before_attr_name(&mut self) {
        match self.input.peek() {
            None | Some('>') => {
                if self.input.peek() == Some('>') { self.input.next_char(); }
                else { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF before attr name"); }
                self.emit_tag();
            }
            Some('/') => {
                self.input.next_char();
                self.state = TokenizerState::SelfClosingStartTag;
            }
            Some(c) if c.is_ascii_whitespace() => { self.input.next_char(); }
            Some(_) => {
                self.tag.cur_attr_start = self.input.location();
                self.tag.cur_attr_name.clear();
                self.tag.cur_attr_value.clear();
                self.state = TokenizerState::AttributeName;
            }
        }
    }

    fn handle_attr_name(&mut self) {
        match self.input.peek() {
            None => {
                self.diag_error(DiagnosticCode::UnterminatedTag, "EOF in attr name");
                self.emit_eof();
            }
            Some('=') => {
                self.input.next_char();
                self.state = TokenizerState::BeforeAttributeValue;
            }
            Some('>') => {
                let end = self.input.location();
                self.tag.flush_current_attr(end);
                self.input.next_char();
                self.emit_tag();
            }
            Some('/') => {
                let end = self.input.location();
                self.tag.flush_current_attr(end);
                self.input.next_char();
                self.state = TokenizerState::SelfClosingStartTag;
            }
            Some(c) if c.is_ascii_whitespace() => {
                self.input.next_char();
                self.state = TokenizerState::AfterAttributeName;
            }
            Some(c) => {
                self.tag.cur_attr_name.push(c.to_ascii_lowercase());
                self.input.next_char();
            }
        }
    }

    fn handle_after_attr_name(&mut self) {
        match self.input.peek() {
            None => { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF after attr"); self.emit_eof(); }
            Some(c) if c.is_ascii_whitespace() => { self.input.next_char(); }
            Some('/') => { self.input.next_char(); self.state = TokenizerState::SelfClosingStartTag; }
            Some('=') => { self.input.next_char(); self.state = TokenizerState::BeforeAttributeValue; }
            Some('>') => {
                let end = self.input.location();
                self.tag.flush_current_attr(end);
                self.input.next_char();
                self.emit_tag();
            }
            Some(_) => {
                let end = self.input.location();
                self.tag.flush_current_attr(end);
                self.tag.cur_attr_start = self.input.location();
                self.state = TokenizerState::AttributeName;
            }
        }
    }

    fn handle_before_attr_value(&mut self) {
        match self.input.peek() {
            Some(c) if c.is_ascii_whitespace() => { self.input.next_char(); }
            Some('"') => { self.input.next_char(); self.state = TokenizerState::AttributeValueDoubleQuoted; }
            Some('\'') => { self.input.next_char(); self.state = TokenizerState::AttributeValueSingleQuoted; }
            Some('>') => {
                self.diag_warn(DiagnosticCode::MalformedAttributeAssignment, "Empty attr value before '>'");
                let end = self.input.location();
                self.tag.flush_current_attr(end);
                self.input.next_char();
                self.emit_tag();
            }
            None => { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF before attr value"); self.emit_eof(); }
            Some(_) => { self.state = TokenizerState::AttributeValueUnquoted; }
        }
    }

    fn handle_attr_value_dquote(&mut self) {
        match self.input.peek() {
            None => { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF in attr"); self.emit_eof(); }
            Some('"') => {
                self.input.next_char();
                self.state = TokenizerState::AfterAttributeValueQuoted;
            }
            Some(c) => { self.tag.cur_attr_value.push(c); self.input.next_char(); }
        }
    }

    fn handle_attr_value_squote(&mut self) {
        match self.input.peek() {
            None => { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF in attr"); self.emit_eof(); }
            Some('\'') => {
                self.input.next_char();
                self.state = TokenizerState::AfterAttributeValueQuoted;
            }
            Some(c) => { self.tag.cur_attr_value.push(c); self.input.next_char(); }
        }
    }

    fn handle_attr_value_unquoted(&mut self) {
        match self.input.peek() {
            None => { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF in attr"); self.emit_eof(); }
            Some(c) if c.is_ascii_whitespace() => {
                let end = self.input.location();
                self.tag.flush_current_attr(end);
                self.input.next_char();
                self.state = TokenizerState::BeforeAttributeName;
            }
            Some('>') => {
                let end = self.input.location();
                self.tag.flush_current_attr(end);
                self.input.next_char();
                self.emit_tag();
            }
            Some(c) => { self.tag.cur_attr_value.push(c); self.input.next_char(); }
        }
    }

    fn handle_after_attr_value_quoted(&mut self) {
        let end = self.input.location();
        self.tag.flush_current_attr(end);
        match self.input.peek() {
            Some(c) if c.is_ascii_whitespace() => { self.input.next_char(); self.state = TokenizerState::BeforeAttributeName; }
            Some('/') => { self.input.next_char(); self.state = TokenizerState::SelfClosingStartTag; }
            Some('>') => { self.input.next_char(); self.emit_tag(); }
            None => { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF after attr value"); self.emit_eof(); }
            Some(_) => { self.state = TokenizerState::BeforeAttributeName; }
        }
    }

    fn handle_self_closing(&mut self) {
        match self.input.peek() {
            Some('>') => {
                self.tag.self_closing = true;
                if self.tag.is_end_tag {
                    self.diag_warn(DiagnosticCode::AttributesInEndTag, "Self-closing end tag");
                }
                self.input.next_char();
                self.emit_tag();
            }
            None => { self.diag_error(DiagnosticCode::UnterminatedTag, "EOF in self-closing tag"); self.emit_eof(); }
            Some(_) => {
                // '/' was not followed by '>' — treat as unquoted attr char
                self.tag.cur_attr_value.push('/');
                self.state = TokenizerState::BeforeAttributeName;
            }
        }
    }

    fn handle_markup_decl_open(&mut self) {
        if self.input.consume_str("--") {
            self.comment_start = self.input.location();
            self.comment_buf.clear();
            self.state = TokenizerState::Comment;
        } else if self.input.consume_str_ci("DOCTYPE") {
            self.doctype_start = self.input.location();
            self.doctype = DoctypeData::default();
            self.state = TokenizerState::Doctype;
        } else {
            // Unknown markup declaration — consume until '>' and warn
            self.diag_warn(DiagnosticCode::UnexpectedCharAfterLt, "Unknown markup declaration");
            while let Some(c) = self.input.peek() {
                self.input.next_char();
                if c == '>' { break; }
            }
            self.state = TokenizerState::Data;
        }
    }

    fn handle_comment(&mut self) {
        match self.input.peek() {
            None => {
                self.diag_error(DiagnosticCode::UnterminatedComment, "EOF in comment");
                self.emit_comment();
                self.emit_eof();
            }
            Some('-') => { self.input.next_char(); self.state = TokenizerState::CommentEndDash; }
            Some(c) => { self.comment_buf.push(c); self.input.next_char(); }
        }
    }

    fn handle_comment_end_dash(&mut self) {
        match self.input.peek() {
            Some('-') => { self.input.next_char(); self.state = TokenizerState::CommentEnd; }
            None => {
                self.diag_error(DiagnosticCode::UnterminatedComment, "EOF in comment");
                self.comment_buf.push('-');
                self.emit_comment();
                self.emit_eof();
            }
            Some(c) => {
                self.comment_buf.push('-');
                self.comment_buf.push(c);
                self.input.next_char();
                self.state = TokenizerState::Comment;
            }
        }
    }

    fn handle_comment_end(&mut self) {
        match self.input.peek() {
            Some('>') => { self.input.next_char(); self.emit_comment(); }
            Some('-') => {
                self.diag_warn(DiagnosticCode::DoubleHyphenInComment, "'--' inside comment");
                self.comment_buf.push('-');
                self.input.next_char();
            }
            None => {
                self.diag_error(DiagnosticCode::UnterminatedComment, "EOF in comment end");
                self.emit_comment();
                self.emit_eof();
            }
            Some(c) => {
                self.comment_buf.push('-');
                self.comment_buf.push('-');
                self.comment_buf.push(c);
                self.input.next_char();
                self.state = TokenizerState::Comment;
            }
        }
    }

    fn handle_doctype(&mut self) {
        match self.input.peek() {
            Some(c) if c.is_ascii_whitespace() => { self.input.next_char(); }
            Some(c) if c.is_ascii_alphabetic() => {
                self.doctype.name = Some(String::new());
                self.state = TokenizerState::DoctypeName;
            }
            Some('>') => {
                self.diag_error(DiagnosticCode::MalformedDoctype, "Empty DOCTYPE");
                self.doctype.force_quirks = true;
                self.input.next_char();
                self.emit_doctype();
            }
            None => {
                self.diag_error(DiagnosticCode::UnexpectedEof, "EOF in DOCTYPE");
                self.doctype.force_quirks = true;
                self.emit_doctype();
                self.emit_eof();
            }
            Some(c) => {
                self.diag_warn(DiagnosticCode::MalformedDoctype, &format!("Unexpected '{}' in DOCTYPE", c));
                self.input.next_char();
            }
        }
    }

    fn handle_doctype_name(&mut self) {
        match self.input.peek() {
            Some(c) if c.is_ascii_whitespace() => {
                self.input.next_char();
                // Skip rest of DOCTYPE until '>'
                while let Some(c) = self.input.peek() {
                    self.input.next_char();
                    if c == '>' { self.emit_doctype(); return; }
                }
                self.emit_doctype();
            }
            Some('>') => { self.input.next_char(); self.emit_doctype(); }
            None => {
                self.diag_error(DiagnosticCode::UnexpectedEof, "EOF in DOCTYPE name");
                self.emit_doctype();
                self.emit_eof();
            }
            Some(c) => {
                if let Some(name) = self.doctype.name.as_mut() {
                    name.push(c.to_ascii_lowercase());
                }
                self.input.next_char();
            }
        }
    }

    // -------------------------------------------------------
    // ScriptData State Handlers
    // -------------------------------------------------------

    fn handle_script_data(&mut self) {
        match self.input.peek() {
            None => {
                self.flush_text();
                self.emit_eof();
            }
            Some('<') => {
                self.input.next_char();
                self.state = TokenizerState::ScriptDataLessThanSign;
            }
            Some('\0') => {
                self.diag_warn(DiagnosticCode::NullCharacterInInput, "Null character in script data");
                self.input.next_char();
                self.text_buf.push('\u{FFFD}');
            }
            Some(ch) => {
                if self.text_buf.is_empty() {
                    self.text_start = self.input.location();
                }
                self.text_buf.push(ch);
                self.input.next_char();
            }
        }
    }

    fn handle_script_data_lt(&mut self) {
        match self.input.peek() {
            Some('/') => {
                self.input.next_char();
                self.state = TokenizerState::ScriptDataEndTagOpen;
            }
            Some('!') => {
                self.text_buf.push('<');
                self.text_buf.push('!');
                self.input.next_char();
                self.state = TokenizerState::ScriptData;
            }
            _ => {
                self.text_buf.push('<');
                self.state = TokenizerState::ScriptData;
            }
        }
    }

    fn handle_script_data_end_tag_open(&mut self) {
        match self.input.peek() {
            Some(c) if c.is_ascii_alphabetic() => {
                self.tag = TagBuilder { is_end_tag: true, start: self.input.location(), ..Default::default() };
                self.state = TokenizerState::ScriptDataEndTagName;
            }
            _ => {
                self.text_buf.push('<');
                self.text_buf.push('/');
                self.state = TokenizerState::ScriptData;
            }
        }
    }

    fn handle_script_data_end_tag_name(&mut self) {
        let is_script = self.tag.name.eq_ignore_ascii_case("script");
        match self.input.peek() {
            Some('>') => {
                if is_script {
                    self.flush_text();
                    self.input.next_char();
                    self.emit_tag();
                } else {
                    self.text_buf.push('<');
                    self.text_buf.push('/');
                    self.text_buf.push_str(&self.tag.name);
                    self.text_buf.push('>');
                    self.input.next_char();
                    self.state = TokenizerState::ScriptData;
                }
            }
            Some(c) if c.is_ascii_alphabetic() => {
                self.tag.name.push(c.to_ascii_lowercase());
                self.input.next_char();
            }
            Some(c) if c.is_ascii_whitespace() => {
                if is_script {
                    self.flush_text();
                    self.input.next_char();
                    self.state = TokenizerState::BeforeAttributeName;
                } else {
                    self.text_buf.push('<');
                    self.text_buf.push('/');
                    self.text_buf.push_str(&self.tag.name);
                    self.text_buf.push(c);
                    self.input.next_char();
                    self.state = TokenizerState::ScriptData;
                }
            }
            _ => {
                self.text_buf.push('<');
                self.text_buf.push('/');
                self.text_buf.push_str(&self.tag.name);
                self.state = TokenizerState::ScriptData;
            }
        }
    }
}
