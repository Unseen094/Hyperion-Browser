use std::ffi::{c_char, CStr};

/// CSS Token Types
#[derive(Debug, Clone, PartialEq)]
pub enum TokenType {
    Identifier,
    AtKeyword,
    Hash,
    String,
    BadString,
    Url,
    BadUrl,
    Delim(char),
    Function,
    UrlFunction,
    Comma,
    Colon,
    Semicolon,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Dimension { value: f64, unit: String },
    Percentage(f64),
    Number(f64),
    Whitespace,
    Comment,
    EndOfFile,
}

/// CSS Token
#[derive(Debug, Clone)]
pub struct Token {
    pub token_type: TokenType,
    pub value: String,
    pub line: usize,
    pub column: usize,
}

/// CSS Selector
#[derive(Debug, Clone)]
pub struct Selector {
    pub tag_name: Option<String>,
    pub id: Option<String>,
    pub classes: Vec<String>,
    pub attributes: Vec<(String, Option<String>)>,
    pub pseudo_class: Option<String>,
    pub pseudo_element: Option<String>,
}

/// CSS Declaration
#[derive(Debug, Clone)]
pub struct Declaration {
    pub property: String,
    pub value: String,
    pub important: bool,
}

/// CSS Rule
#[derive(Debug, Clone)]
pub struct Rule {
    pub selectors: Vec<Selector>,
    pub declarations: Vec<Declaration>,
}

/// CSS Stylesheet
#[derive(Debug)]
pub struct Stylesheet {
    pub rules: Vec<Rule>,
}

impl Stylesheet {
    pub fn new() -> Self {
        Stylesheet { rules: Vec::new() }
    }
}

/// Lexer for CSS
pub struct Lexer {
    input: Vec<char>,
    pos: usize,
    line: usize,
    column: usize,
}

impl Lexer {
    pub fn new(input: &str) -> Self {
        Lexer {
            input: input.chars().collect(),
            pos: 0,
            line: 1,
            column: 1,
        }
    }

    fn peek(&self) -> Option<char> {
        if self.pos < self.input.len() {
            Some(self.input[self.pos])
        } else {
            None
        }
    }

    fn peek_next(&self) -> Option<char> {
        if self.pos + 1 < self.input.len() {
            Some(self.input[self.pos + 1])
        } else {
            None
        }
    }

    fn advance(&mut self) -> Option<char> {
        if self.pos < self.input.len() {
            let ch = self.input[self.pos];
            self.pos += 1;
            if ch == '\n' {
                self.line += 1;
                self.column = 1;
            } else {
                self.column += 1;
            }
            Some(ch)
        } else {
            None
        }
    }

    fn skip_whitespace(&mut self) {
        while let Some(ch) = self.peek() {
            if ch.is_whitespace() {
                self.advance();
            } else {
                break;
            }
        }
    }

    fn read_string(&mut self, quote: char) -> String {
        let mut result = String::new();
        while let Some(ch) = self.advance() {
            if ch == quote {
                break;
            }
            if ch == '\\' {
                // Handle escape sequences
                if let Some(escaped) = self.advance() {
                    result.push(escaped);
                }
            } else {
                result.push(ch);
            }
        }
        result
    }

    fn read_identifier(&mut self, start: char) -> String {
        let mut result = String::new();
        result.push(start);
        while let Some(ch) = self.peek() {
            if ch.is_alphanumeric() || ch == '-' || ch == '_' {
                result.push(self.advance().unwrap());
            } else {
                break;
            }
        }
        result
    }

    fn read_number(&mut self, start: &str) -> (f64, String) {
        let mut result = String::from(start);
        
        // Read integer part
        while let Some(ch) = self.peek() {
            if ch.is_ascii_digit() {
                result.push(self.advance().unwrap());
            } else {
                break;
            }
        }
        
        // Read decimal part
        if self.peek() == Some('.') {
            result.push(self.advance().unwrap());
            while let Some(ch) = self.peek() {
                if ch.is_ascii_digit() {
                    result.push(self.advance().unwrap());
                } else {
                    break;
                }
            }
        }
        
        // Read unit (if any)
        let mut unit = String::new();
        while let Some(ch) = self.peek() {
            if ch.is_ascii_alphabetic() || ch == '%' {
                unit.push(self.advance().unwrap());
            } else {
                break;
            }
        }
        
        let value: f64 = result.parse().unwrap_or(0.0);
        (value, unit)
    }

    pub fn tokenize(&mut self) -> Vec<Token> {
        let mut tokens = Vec::new();
        
        while let Some(ch) = self.peek() {
            if ch.is_whitespace() {
                self.skip_whitespace();
                continue;
            }
            
            let line = self.line;
            let column = self.column;
            
            let token = match ch {
                '#' => {
                    self.advance();
                    let next_ch = self.advance().unwrap_or('#');
                    let value = self.read_identifier(next_ch);
                    Token {
                        token_type: TokenType::Hash,
                        value,
                        line,
                        column,
                    }
                }
                '.' => {
                    self.advance();
                    if let Some(next) = self.peek() {
                        if next.is_ascii_digit() {
                            let (value, unit) = self.read_number(".");
                            let token_type = if unit.is_empty() {
                                TokenType::Number(value)
                            } else {
                                TokenType::Dimension { value, unit }
                            };
                            Token { token_type, value: String::new(), line, column }
                        } else {
                            Token {
                                token_type: TokenType::Delim('.'),
                                value: String::from("."),
                                line,
                                column,
                            }
                        }
                    } else {
                        Token {
                            token_type: TokenType::Delim('.'),
                            value: String::from("."),
                            line,
                            column,
                        }
                    }
                }
                '"' | '\'' => {
                    let quote = self.advance().unwrap();
                    let value = self.read_string(quote);
                    Token {
                        token_type: TokenType::String,
                        value,
                        line,
                        column,
                    }
                }
                '@' => {
                    self.advance();
                    let next_ch = self.advance().unwrap_or('@');
                    let value = self.read_identifier(next_ch);
                    Token {
                        token_type: TokenType::AtKeyword,
                        value,
                        line,
                        column,
                    }
                }
                ':' => {
                    self.advance();
                    Token {
                        token_type: TokenType::Colon,
                        value: String::from(":"),
                        line,
                        column,
                    }
                }
                ';' => {
                    self.advance();
                    Token {
                        token_type: TokenType::Semicolon,
                        value: String::from(";"),
                        line,
                        column,
                    }
                }
                '{' => {
                    self.advance();
                    Token {
                        token_type: TokenType::LeftBrace,
                        value: String::from("{"),
                        line,
                        column,
                    }
                }
                '}' => {
                    self.advance();
                    Token {
                        token_type: TokenType::RightBrace,
                        value: String::from("}"),
                        line,
                        column,
                    }
                }
                ',' => {
                    self.advance();
                    Token {
                        token_type: TokenType::Comma,
                        value: String::from(","),
                        line,
                        column,
                    }
                }
                _ => {
                    let value = self.read_identifier(ch);
                    Token {
                        token_type: TokenType::Identifier,
                        value,
                        line,
                        column,
                    }
                }
            };
            
            tokens.push(token);
        }
        
        tokens.push(Token {
            token_type: TokenType::EndOfFile,
            value: String::new(),
            line: self.line,
            column: self.column,
        });
        
        tokens
    }
}

/// Parse CSS selectors
fn parse_selector(selector_str: &str) -> Selector {
    let mut selector = Selector {
        tag_name: None,
        id: None,
        classes: Vec::new(),
        attributes: Vec::new(),
        pseudo_class: None,
        pseudo_element: None,
    };
    
    let mut current = String::new();
    let mut in_attribute = false;
    let mut attribute_name = String::new();
    
    for ch in selector_str.chars() {
        match ch {
            '#' => {
                if !current.is_empty() {
                    if current.chars().next().map_or(false, |c| c.is_ascii_digit()) {
                        // Tag name
                        selector.tag_name = Some(current.clone());
                    }
                    current.clear();
                }
            }
            '.' => {
                if !current.is_empty() {
                    if current.starts_with('#') {
                        selector.id = Some(current.trim_start_matches('#').to_string());
                    } else {
                        selector.classes.push(current.clone());
                    }
                    current.clear();
                }
            }
            '[' => {
                in_attribute = true;
                attribute_name.clear();
            }
            ']' => {
                in_attribute = false;
                if !attribute_name.is_empty() {
                    selector.attributes.push((attribute_name.clone(), None));
                    attribute_name.clear();
                }
            }
            ':' => {
                if !current.is_empty() {
                    if current.starts_with(':') {
                        selector.pseudo_element = Some(current.trim_start_matches(':').to_string());
                    } else {
                        selector.pseudo_class = Some(current.trim_start_matches(':').to_string());
                    }
                    current.clear();
                }
            }
            '=' if in_attribute => {
                // Handle attribute value
            }
            _ => {
                if in_attribute {
                    attribute_name.push(ch);
                } else {
                    current.push(ch);
                }
            }
        }
    }
    
    // Handle remaining content
    if !current.is_empty() {
        if current.starts_with('#') {
            selector.id = Some(current.trim_start_matches('#').to_string());
        } else if current.starts_with('.') {
            selector.classes.push(current.trim_start_matches('.').to_string());
        } else if current.starts_with(':') {
            selector.pseudo_class = Some(current.trim_start_matches(':').to_string());
        } else {
            selector.tag_name = Some(current);
        }
    }
    
    selector
}

/// Parse a CSS stylesheet
pub fn parse_stylesheet(css: &str) -> Stylesheet {
    let mut lexer = Lexer::new(css);
    let tokens = lexer.tokenize();
    let mut stylesheet = Stylesheet::new();
    
    // Simple parser implementation
    let mut current_selectors = Vec::new();
    let mut current_declarations = Vec::new();
    let mut in_block = false;
    
    for token in tokens {
        match token.token_type {
            TokenType::Identifier | TokenType::Hash | TokenType::Delim('.') => {
                if !in_block {
                    // Collect selectors
                    current_selectors.push(parse_selector(&token.value));
                }
            }
            TokenType::LeftBrace => {
                in_block = true;
            }
            TokenType::RightBrace => {
                in_block = false;
                if !current_selectors.is_empty() && !current_declarations.is_empty() {
                    let rule = Rule {
                        selectors: current_selectors.clone(),
                        declarations: current_declarations.clone(),
                    };
                    stylesheet.rules.push(rule);
                    current_selectors.clear();
                    current_declarations.clear();
                }
            }
            _ => {}
        }
    }
    
    stylesheet
}

// FFI Functions for C++ integration
#[no_mangle]
pub extern "C" fn rust_parse_css(css_input: *const c_char) -> *mut c_char {
    if css_input.is_null() {
        return std::ptr::null_mut();
    }
    
    let css_str = unsafe {
        CStr::from_ptr(css_input)
            .to_string_lossy()
            .into_owned()
    };
    
    let stylesheet = parse_stylesheet(&css_str);
    
    // Convert to JSON-like string for C++ consumption
    let mut result = String::from("{ \"rules\": [");
    let mut first_rule = true;
    
    for rule in &stylesheet.rules {
        if !first_rule {
            result.push_str(", ");
        }
        first_rule = false;
        
        result.push_str("{ \"selectors\": [");
        for (i, selector) in rule.selectors.iter().enumerate() {
            if i > 0 { result.push_str(", "); }
            if let Some(ref tag) = selector.tag_name {
                result.push_str(&format!("\"{}\"", tag));
            }
        }
        result.push_str("], \"declarations\": [");
        
        for (i, decl) in rule.declarations.iter().enumerate() {
            if i > 0 { result.push_str(", "); }
            result.push_str(&format!("\"{}: {}\"", decl.property, decl.value));
        }
        
        result.push_str("] }");
    }
    
    result.push_str("] }");
    
    // Convert to C string
    let c_str = std::ffi::CString::new(result).unwrap();
    c_str.into_raw()
}

#[no_mangle]
pub extern "C" fn rust_free_css_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = std::ffi::CString::from_raw(s);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_selector() {
        let selector = parse_selector("div");
        assert_eq!(selector.tag_name, Some("div".to_string()));
    }

    #[test]
    fn test_class_selector() {
        let selector = parse_selector(".container");
        assert_eq!(selector.classes, vec!["container".to_string()]);
    }

    #[test]
    fn test_id_selector() {
        let selector = parse_selector("#main");
        assert_eq!(selector.id, Some("main".to_string()));
    }
}
