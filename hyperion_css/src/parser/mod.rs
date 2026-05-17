// ============================================================
// Hyperion CSS Parser — Core Parser Loop (Final Browser Grade)
// ============================================================

use crate::tokenizer::{CssToken, CssTokenKind};
use crate::diagnostics::{DiagnosticBag, SourceSpan};
use crate::stylesheet::Stylesheet;
use crate::rules::Rule;
use crate::selectors::{SelectorList, ComplexSelector, SimpleSelector, Combinator, SelectorPart};
use crate::declarations::Declaration;
use crate::values::CssValue;

pub struct CssParser {
    tokens: Vec<CssToken>,
    cursor: usize,
    pub diagnostics: DiagnosticBag,
}

impl CssParser {
    pub fn new(tokens: Vec<CssToken>, diagnostics: DiagnosticBag) -> Self {
        Self {
            tokens,
            cursor: 0,
            diagnostics,
        }
    }

    pub fn parse_stylesheet(&mut self) -> Stylesheet {
        let mut stylesheet = Stylesheet::new();

        while !self.is_eof() {
            self.skip_whitespace_and_comments();
            if self.is_eof() { break; }

            if let Some(rule) = self.parse_rule() {
                stylesheet.rules.push(rule);
            } else {
                self.recover_to_next_rule();
            }
        }

        stylesheet
    }

    // ---- Parser Helpers ----

    fn peek(&self) -> Option<&CssToken> {
        self.tokens.get(self.cursor)
    }

    fn advance(&mut self) {
        if self.cursor < self.tokens.len() {
            self.cursor += 1;
        }
    }

    fn is_eof(&self) -> bool {
        self.cursor >= self.tokens.len() || self.peek().unwrap().kind == CssTokenKind::Eof
    }

    fn skip_whitespace_and_comments(&mut self) {
        while let Some(tok) = self.peek() {
            match tok.kind {
                CssTokenKind::Whitespace | CssTokenKind::Comment(_) => self.advance(),
                _ => break,
            }
        }
    }

    // ---- Rule Parsing ----

    fn parse_rule(&mut self) -> Option<Rule> {
        self.skip_whitespace_and_comments();
        let tok = self.peek()?;

        match &tok.kind {
            CssTokenKind::Delim('@') => self.parse_at_rule(),
            _ => self.parse_style_rule(),
        }
    }

    fn parse_at_rule(&mut self) -> Option<Rule> {
        self.advance(); // '@'
        let name = if let Some(CssTokenKind::Identifier(n)) = self.peek().map(|t| &t.kind) {
            let res = n.clone();
            self.advance();
            res
        } else {
            return None;
        };

        let mut prelude = String::new();
        let mut block = None;

        loop {
            self.skip_whitespace_and_comments();
            match self.peek().map(|t| &t.kind) {
                Some(CssTokenKind::CurlyOpen) => {
                    self.advance();
                    let mut inner_rules = Vec::new();
                    loop {
                        self.skip_whitespace_and_comments();
                        if self.is_eof() || self.peek()?.kind == CssTokenKind::CurlyClose {
                            break;
                        }
                        if let Some(rule) = self.parse_rule() {
                            inner_rules.push(rule);
                        } else {
                            self.recover_to_next_rule();
                        }
                    }
                    if let Some(tok) = self.peek() {
                        if tok.kind == CssTokenKind::CurlyClose { self.advance(); }
                    }
                    block = Some(inner_rules);
                    break;
                }
                Some(CssTokenKind::Semicolon) => {
                    self.advance();
                    break;
                }
                Some(CssTokenKind::Eof) => break,
                Some(_) => {
                    // Very simplistic prelude capture
                    prelude.push_str("..."); // placeholder for complex prelude parsing
                    self.advance();
                }
                None => break,
            }
        }

        Some(Rule::AtRule { name, prelude, block })
    }

    fn parse_style_rule(&mut self) -> Option<Rule> {
        let selectors = self.parse_selector_list()?;
        
        self.skip_whitespace_and_comments();
        
        if let Some(tok) = self.peek() {
            if tok.kind != CssTokenKind::CurlyOpen {
                self.diagnostics.error(tok.span, "Expected '{' after selector");
                return None;
            }
            self.advance(); // consume '{'
        } else {
            return None;
        }

        let mut declarations = Vec::new();
        
        loop {
            self.skip_whitespace_and_comments();
            if self.is_eof() { break; }
            
            if let Some(tok) = self.peek() {
                if tok.kind == CssTokenKind::CurlyClose {
                    self.advance(); // consume '}'
                    break;
                }
            }

            if let Some(decl) = self.parse_declaration() {
                declarations.push(decl);
            } else {
                self.recover_to_next_declaration();
            }
        }

        Some(Rule::StyleRule { selectors, declarations })
    }

    fn recover_to_next_rule(&mut self) {
        let mut depth = 0;
        while let Some(tok) = self.peek() {
            match tok.kind {
                CssTokenKind::CurlyOpen => {
                    depth += 1;
                    self.advance();
                }
                CssTokenKind::CurlyClose => {
                    self.advance();
                    if depth == 0 { break; }
                    depth -= 1;
                    if depth == 0 { break; }
                }
                CssTokenKind::Eof => break,
                _ => self.advance(),
            }
        }
    }

    fn recover_to_next_declaration(&mut self) {
        while let Some(tok) = self.peek() {
            match tok.kind {
                CssTokenKind::Semicolon => {
                    self.advance();
                    break;
                }
                CssTokenKind::CurlyClose | CssTokenKind::Eof => break,
                _ => self.advance(),
            }
        }
    }

    // ---- Selector Parsing ----

    fn parse_selector_list(&mut self) -> Option<SelectorList> {
        let mut selectors = Vec::new();
        
        loop {
            self.skip_whitespace_and_comments();
            if self.is_eof() { break; }
            if let Some(tok) = self.peek() {
                if tok.kind == CssTokenKind::CurlyOpen { break; }
            }

            if let Some(complex) = self.parse_complex_selector() {
                selectors.push(complex);
            } else {
                break;
            }

            self.skip_whitespace_and_comments();
            if let Some(tok) = self.peek() {
                if tok.kind == CssTokenKind::Comma {
                    self.advance();
                    continue;
                }
            }
            break;
        }

        if selectors.is_empty() { None } else { Some(SelectorList { selectors }) }
    }

    fn parse_complex_selector(&mut self) -> Option<ComplexSelector> {
        let mut parts = Vec::new();

        loop {
            let mut skipped_whitespace = false;
            while let Some(tok) = self.peek() {
                match tok.kind {
                    CssTokenKind::Whitespace => {
                        skipped_whitespace = true;
                        self.advance();
                    }
                    CssTokenKind::Comment(_) => self.advance(),
                    _ => break,
                }
            }
            
            if let Some(tok) = self.peek() {
                match tok.kind {
                    CssTokenKind::Delim('>') => {
                        self.advance();
                        parts.push(SelectorPart::Combinator(Combinator::Child));
                        self.skip_whitespace_and_comments();
                        skipped_whitespace = false;
                    }
                    CssTokenKind::Delim('+') => {
                        self.advance();
                        parts.push(SelectorPart::Combinator(Combinator::NextSibling));
                        self.skip_whitespace_and_comments();
                        skipped_whitespace = false;
                    }
                    CssTokenKind::Delim('~') => {
                        self.advance();
                        parts.push(SelectorPart::Combinator(Combinator::SubsequentSibling));
                        self.skip_whitespace_and_comments();
                        skipped_whitespace = false;
                    }
                    CssTokenKind::CurlyOpen | CssTokenKind::Comma => break,
                    _ => {}
                }
            }

            if skipped_whitespace && !parts.is_empty() {
                if let Some(SelectorPart::Compound(_)) = parts.last() {
                    parts.push(SelectorPart::Combinator(Combinator::Descendant));
                }
            }

            let compound = self.parse_compound_selector();
            if compound.is_empty() { break; }
            parts.push(SelectorPart::Compound(compound));
        }

        if parts.is_empty() { None } else { Some(ComplexSelector { parts }) }
    }

    fn parse_compound_selector(&mut self) -> Vec<SimpleSelector> {
        let mut simple = Vec::new();
        while let Some(tok) = self.peek() {
            match &tok.kind {
                CssTokenKind::Identifier(name) => {
                    simple.push(SimpleSelector::TagName(name.clone()));
                    self.advance();
                }
                CssTokenKind::Dot => {
                    self.advance();
                    if let Some(CssTokenKind::Identifier(c)) = self.peek().map(|t| &t.kind) {
                        let cname = c.clone();
                        self.advance();
                        simple.push(SimpleSelector::Class(cname));
                    }
                }
                CssTokenKind::Hash(id) => {
                    let id_name = id.clone();
                    self.advance();
                    simple.push(SimpleSelector::Id(id_name));
                }
                CssTokenKind::Colon => {
                    self.advance();
                    if let Some(CssTokenKind::Identifier(p)) = self.peek().map(|t| &t.kind) {
                        let pname = p.clone();
                        self.advance();
                        simple.push(SimpleSelector::PseudoClass(pname));
                    }
                }
                CssTokenKind::BracketOpen => {
                    self.advance();
                    let mut name = String::new();
                    if let Some(CssTokenKind::Identifier(n)) = self.peek().map(|t| &t.kind) {
                        name = n.clone();
                        self.advance();
                    }
                    // Simple attribute parser: [name="value"]
                    let mut operator = None;
                    let mut value = None;
                    if let Some(CssTokenKind::Delim('=')) = self.peek().map(|t| &t.kind) {
                        operator = Some("=".to_string());
                        self.advance();
                        if let Some(CssTokenKind::String(s)) = self.peek().map(|t| &t.kind) {
                            value = Some(s.clone());
                            self.advance();
                        }
                    }
                    if let Some(CssTokenKind::BracketClose) = self.peek().map(|t| &t.kind) {
                        self.advance();
                    }
                    simple.push(SimpleSelector::Attribute { name, operator, value });
                }
                _ => break,
            }
        }
        simple
    }

    // ---- Declaration Parsing ----

    fn parse_declaration(&mut self) -> Option<Declaration> {
        self.skip_whitespace_and_comments();
        
        let prop_name = if let Some(CssTokenKind::Identifier(name)) = self.peek().map(|t| &t.kind) {
            let res = name.clone();
            self.advance();
            res
        } else {
            return None;
        };

        self.skip_whitespace_and_comments();
        if let Some(tok) = self.peek() {
            if tok.kind != CssTokenKind::Colon {
                self.diagnostics.error(tok.span, format!("Expected ':' after property '{}'", prop_name));
                return None;
            }
            self.advance();
        } else { return None; }

        self.skip_whitespace_and_comments();
        let value = self.parse_value_list()?;
        
        self.skip_whitespace_and_comments();
        let mut is_important = false;
        if let Some(CssTokenKind::Delim('!')) = self.peek().map(|t| &t.kind) {
            self.advance();
            if let Some(CssTokenKind::Identifier(i)) = self.peek().map(|t| &t.kind) {
                if i.eq_ignore_ascii_case("important") {
                    is_important = true;
                    self.advance();
                }
            }
        }

        self.skip_whitespace_and_comments();
        if let Some(tok) = self.peek() {
            if tok.kind == CssTokenKind::Semicolon { self.advance(); }
        }

        Some(Declaration::new(prop_name, value, is_important))
    }

    fn parse_value_list(&mut self) -> Option<CssValue> {
        let mut vals = Vec::new();
        loop {
            self.skip_whitespace_and_comments();
            if let Some(tok) = self.peek() {
                if matches!(tok.kind, CssTokenKind::Semicolon | CssTokenKind::CurlyClose | CssTokenKind::Eof) {
                    break;
                }
                if tok.kind == CssTokenKind::Delim('!') { break; }
            } else { break; }

            if let Some(val) = self.parse_value() {
                vals.push(val);
            } else {
                self.advance();
            }
        }

        match vals.len() {
            0 => None,
            1 => Some(vals.into_iter().next().unwrap()),
            _ => Some(CssValue::List(vals)),
        }
    }

    fn parse_value(&mut self) -> Option<CssValue> {
        let tok = self.peek()?;
        match &tok.kind {
            CssTokenKind::Identifier(name) => {
                let res = name.clone();
                self.advance();
                Some(CssValue::Keyword(res))
            }
            CssTokenKind::Function(name) => {
                let fname = name.clone();
                self.advance();
                let mut args = Vec::new();
                loop {
                    self.skip_whitespace_and_comments();
                    if let Some(tok) = self.peek() {
                        if tok.kind == CssTokenKind::ParenClose {
                            self.advance();
                            break;
                        }
                        if tok.kind == CssTokenKind::Comma {
                            self.advance();
                            continue;
                        }
                    } else { break; }

                    if let Some(arg) = self.parse_value() {
                        args.push(arg);
                    } else { self.advance(); }
                }
                Some(CssValue::Function(fname, args))
            }
            CssTokenKind::Number(n) => {
                let val = n.parse().unwrap_or(0.0);
                self.advance();
                Some(CssValue::Number(val))
            }
            CssTokenKind::Dimension { value, unit } => {
                let val = value.parse().unwrap_or(0.0);
                let u = unit.clone();
                self.advance();
                Some(CssValue::Length(val, u))
            }
            CssTokenKind::Percentage(p) => {
                let val = p.parse().unwrap_or(0.0);
                self.advance();
                Some(CssValue::Percentage(val))
            }
            CssTokenKind::Hash(h) => {
                let hex = format!("#{}", h);
                self.advance();
                Some(CssValue::Color(hex))
            }
            CssTokenKind::String(s) => {
                let res = s.clone();
                self.advance();
                Some(CssValue::String(res))
            }
            _ => None,
        }
    }
}
