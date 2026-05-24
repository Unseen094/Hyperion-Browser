use std::str::Chars;
use std::iter::Peekable;

#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    Identifier(String),
    Number(f64),
    String(String),
    TemplateString(String),
    True,
    False,
    Null,
    Undefined,
    Var,
    Let,
    Const,
    Fun,
    Async,
    Await,
    Yield,
    Return,
    If,
    Else,
    For,
    While,
    Do,
    Switch,
    Case,
    Default,
    Break,
    Continue,
    Throw,
    Try,
    Catch,
    Finally,
    Class,
    Extends,
    Super,
    New,
    This,
    Import,
    Export,
    From,
    As,
    Static,
    Get,
    Set,
    Delete,
    Typeof,
    In,
    Instanceof,
    Void,
    Of,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    SemiColon,
    Colon,
    Comma,
    Dot,
    QuestionMark,
    Arrow,
    Equal,
    EqualEqual,
    EqualEqualEqual,
    BangEqual,
    BangEqualEqual,
    Plus,
    PlusPlus,
    PlusEqual,
    Minus,
    MinusMinus,
    MinusEqual,
    Star,
    StarEqual,
    StarStar,
    StarStarEqual,
    Slash,
    SlashEqual,
    Percent,
    PercentEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Pipe,
    PipeEqual,
    PipePipe,
    Amp,
    AmpEqual,
    AmpAmp,
    Caret,
    CaretEqual,
    QuestionDot,
    QuestionQuestion,
    QuestionQuestionEqual,
    LeftParenEqual,
    DotDotDot,
    EndOfFile,
}

#[derive(Debug, Clone)]
pub struct TokenWithPosition {
    pub token: Token,
    pub start: usize,
    pub end: usize,
    pub line: usize,
    pub column: usize,
}

pub struct Lexer<'a> {
    input: Peekable<Chars<'a>>,
    pos: usize,
    line: usize,
    column: usize,
    start_pos: usize,
    start_line: usize,
    start_column: usize,
}

impl<'a> Lexer<'a> {
    pub fn new(source: &'a str) -> Self {
        Lexer {
            input: source.chars().peekable(),
            pos: 0,
            line: 1,
            column: 1,
            start_pos: 0,
            start_line: 1,
            start_column: 1,
        }
    }

    fn advance(&mut self) -> Option<char> {
        let ch = self.input.next();
        if let Some(c) = ch {
            self.pos += 1;
            if c == '\n' {
                self.line += 1;
                self.column = 1;
            } else {
                self.column += 1;
            }
        }
        ch
    }

    fn peek(&mut self) -> Option<&char> {
        self.input.peek()
    }

    fn skip_whitespace(&mut self) {
        while let Some(&ch) = self.peek() {
            if ch.is_whitespace() {
                self.advance();
            } else {
                break;
            }
        }
    }

    fn skip_line_comment(&mut self) {
        while let Some(&ch) = self.peek() {
            if ch == '\n' {
                break;
            }
            self.advance();
        }
    }

    fn skip_block_comment(&mut self) -> bool {
        self.advance();
        while let Some(&ch) = self.peek() {
            self.advance();
            if ch == '*' {
                if let Some(&'/') = self.peek() {
                    self.advance();
                    return true;
                }
            }
        }
        false
    }

    fn make_token(&self, token: Token) -> TokenWithPosition {
        TokenWithPosition {
            token,
            start: self.start_pos,
            end: self.pos,
            line: self.start_line,
            column: self.start_column,
        }
    }

    fn scan_identifier_or_keyword(&mut self, first: char) -> TokenWithPosition {
        let mut result = String::new();
        result.push(first);

        while let Some(&ch) = self.peek() {
            if ch.is_alphanumeric() || ch == '_' || ch == '$' {
                result.push(self.advance().unwrap());
            } else {
                break;
            }
        }

        let token = match result.as_str() {
            "true" => Token::True,
            "false" => Token::False,
            "null" => Token::Null,
            "undefined" => Token::Undefined,
            "var" => Token::Var,
            "let" => Token::Let,
            "const" => Token::Const,
            "function" => Token::Fun,
            "async" => Token::Async,
            "await" => Token::Await,
            "yield" => Token::Yield,
            "return" => Token::Return,
            "if" => Token::If,
            "else" => Token::Else,
            "for" => Token::For,
            "while" => Token::While,
            "do" => Token::Do,
            "switch" => Token::Switch,
            "case" => Token::Case,
            "default" => Token::Default,
            "break" => Token::Break,
            "continue" => Token::Continue,
            "throw" => Token::Throw,
            "try" => Token::Try,
            "catch" => Token::Catch,
            "finally" => Token::Finally,
            "class" => Token::Class,
            "extends" => Token::Extends,
            "super" => Token::Super,
            "new" => Token::New,
            "this" => Token::This,
            "import" => Token::Import,
            "export" => Token::Export,
            "from" => Token::From,
            "as" => Token::As,
            "static" => Token::Static,
            "get" => Token::Get,
            "set" => Token::Set,
            "delete" => Token::Delete,
            "typeof" => Token::Typeof,
            "in" => Token::In,
            "instanceof" => Token::Instanceof,
            "void" => Token::Void,
            "of" => Token::Of,
            _ => Token::Identifier(result),
        };

        self.make_token(token)
    }

    fn scan_number(&mut self, first: char) -> TokenWithPosition {
        let mut result = String::new();
        result.push(first);

        while let Some(&ch) = self.peek() {
            if ch.is_ascii_digit() {
                result.push(self.advance().unwrap());
            } else if ch == '.' && result.contains('.') == false {
                result.push(self.advance().unwrap());
            } else if (ch == 'e' || ch == 'E') && result.contains('e') == false {
                result.push(self.advance().unwrap());
                if let Some(&sign) = self.peek() {
                    if sign == '+' || sign == '-' {
                        result.push(self.advance().unwrap());
                    }
                }
            } else {
                break;
            }
        }

        let num: f64 = result.parse().unwrap_or(0.0);
        self.make_token(Token::Number(num))
    }

    fn scan_string(&mut self, quote: char) -> TokenWithPosition {
        let mut result = String::new();

        while let Some(&ch) = self.peek() {
            if ch == quote {
                self.advance();
                return self.make_token(Token::String(result));
            } else if ch == '\\' {
                self.advance();
                if let Some(&escaped) = self.peek() {
                    match escaped {
                        'n' => result.push('\n'),
                        'r' => result.push('\r'),
                        't' => result.push('\t'),
                        '0' => result.push('\0'),
                        '\\' => result.push('\\'),
                        '\'' => result.push('\''),
                        '"' => result.push('"'),
                        '`' => result.push('`'),
                        '$' => result.push('$'),
                        'u' => {
                            let mut hex = String::new();
                            for _ in 0..4 {
                                if let Some(&h) = self.peek() {
                                    hex.push(h);
                                    self.advance();
                                }
                            }
                            if let Ok(c) = u16::from_str_radix(&hex, 16) {
                                result.push(char::from(c));
                            }
                        }
                        _ => result.push(escaped),
                    }
                    self.advance();
                }
            } else if ch == '\n' {
                break;
            } else {
                result.push(self.advance().unwrap());
            }
        }

        self.make_token(Token::String(result))
    }

    fn scan_template_string(&mut self) -> TokenWithPosition {
        let mut result = String::new();
        let mut brace_level = 0;

        while let Some(&ch) = self.peek() {
            if ch == '`' {
                self.advance();
                return self.make_token(Token::TemplateString(result));
            } else if ch == '\\' {
                self.advance();
                if let Some(&escaped) = self.peek() {
                    result.push(escaped);
                    self.advance();
                }
            } else if ch == '$' {
                self.advance();
                if let Some(&'{') = self.peek() {
                    brace_level += 1;
                    result.push(self.advance().unwrap());
                } else {
                    result.push('$');
                }
            } else if ch == '}' && brace_level > 0 {
                brace_level -= 1;
                result.push(self.advance().unwrap());
            } else {
                result.push(self.advance().unwrap());
            }
        }

        self.make_token(Token::TemplateString(result))
    }

    pub fn next_token(&mut self) -> TokenWithPosition {
        self.skip_whitespace();
        self.start_pos = self.pos;
        self.start_line = self.line;
        self.start_column = self.column;

        if let Some(&ch) = self.peek() {
            if ch.is_alphabetic() || ch == '_' || ch == '$' {
                self.advance();
                return self.scan_identifier_or_keyword(ch);
            }
            if ch.is_ascii_digit() {
                self.advance();
                return self.scan_number(ch);
            }
        }

        let token = match self.advance() {
            Some('(') => Token::LeftParen,
            Some(')') => Token::RightParen,
            Some('{') => Token::LeftBrace,
            Some('}') => Token::RightBrace,
            Some('[') => Token::LeftBracket,
            Some(']') => Token::RightBracket,
            Some(';') => Token::SemiColon,
            Some(':') => Token::Colon,
            Some(',') => Token::Comma,
            Some('.') => {
                if let Some(&'.') = self.peek() {
                    self.advance();
                    if let Some(&'.') = self.peek() {
                        self.advance();
                        return self.make_token(Token::DotDotDot);
                    }
                }
                Token::Dot
            },
            Some('?') => {
                if let Some(&'.') = self.peek() {
                    self.advance();
                    return self.make_token(Token::QuestionDot);
                }
                if let Some(&'?') = self.peek() {
                    self.advance();
                    if let Some(&'=') = self.peek() {
                        self.advance();
                        return self.make_token(Token::QuestionQuestionEqual);
                    }
                    return self.make_token(Token::QuestionQuestion);
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::QuestionMark);
                }
                Token::QuestionMark
            },
            Some('=') => {
                if let Some(&'=') = self.peek() {
                    self.advance();
                    if let Some(&'=') = self.peek() {
                        self.advance();
                        return self.make_token(Token::EqualEqualEqual);
                    }
                    return self.make_token(Token::EqualEqual);
                }
                if let Some(&'>') = self.peek() {
                    self.advance();
                    return self.make_token(Token::Arrow);
                }
                Token::Equal
            },
            Some('!') => {
                if let Some(&'=') = self.peek() {
                    self.advance();
                    if let Some(&'=') = self.peek() {
                        self.advance();
                        return self.make_token(Token::BangEqualEqual);
                    }
                    return self.make_token(Token::BangEqual);
                }
                Token::Bang
            },
            Some('+') => {
                if let Some(&'+') = self.peek() {
                    self.advance();
                    return self.make_token(Token::PlusPlus);
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::PlusEqual);
                }
                Token::Plus
            },
            Some('-') => {
                if let Some(&'-') = self.peek() {
                    self.advance();
                    return self.make_token(Token::MinusMinus);
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::MinusEqual);
                }
                Token::Minus
            },
            Some('*') => {
                if let Some(&'*') = self.peek() {
                    self.advance();
                    if let Some(&'=') = self.peek() {
                        self.advance();
                        return self.make_token(Token::StarStarEqual);
                    }
                    return self.make_token(Token::StarStar);
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::StarEqual);
                }
                Token::Star
            },
            Some('/') => {
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::SlashEqual);
                }
                if let Some(&'/') = self.peek() {
                    self.skip_line_comment();
                    return self.next_token();
                }
                if let Some(&'*') = self.peek() {
                    self.skip_block_comment();
                    return self.next_token();
                }
                Token::Slash
            },
            Some('%') => {
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::PercentEqual);
                }
                Token::Percent
            },
            Some('>') => {
                if let Some(&'>') = self.peek() {
                    self.advance();
                    if let Some(&'>') = self.peek() {
                        self.advance();
                    }
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::GreaterEqual);
                }
                Token::Greater
            },
            Some('<') => {
                if let Some(&'<') = self.peek() {
                    self.advance();
                    if let Some(&'=') = self.peek() {
                        self.advance();
                        return self.make_token(Token::LessEqual);
                    }
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::LessEqual);
                }
                Token::Less
            },
            Some('|') => {
                if let Some(&'|') = self.peek() {
                    self.advance();
                    return self.make_token(Token::PipePipe);
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::PipeEqual);
                }
                Token::Pipe
            },
            Some('&') => {
                if let Some(&'&') = self.peek() {
                    self.advance();
                    return self.make_token(Token::AmpAmp);
                }
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::AmpEqual);
                }
                Token::Amp
            },
            Some('^') => {
                if let Some(&'=') = self.peek() {
                    self.advance();
                    return self.make_token(Token::CaretEqual);
                }
                Token::Caret
            },
            Some('"') | Some('\'') => return self.scan_string(ch),
            Some('`') => return self.scan_template_string(),
            Some(c) => {
                if c.is_whitespace() {
                    return self.next_token();
                }
                Token::Identifier(c.to_string())
            },
            None => Token::EndOfFile,
        };

        self.make_token(token)
    }

    pub fn tokenize(&mut self) -> Vec<TokenWithPosition> {
        let mut tokens = Vec::new();
        loop {
            let token = self.next_token();
            if token.token == Token::EndOfFile {
                tokens.push(token);
                break;
            }
            tokens.push(token);
        }
        tokens
    }
}

#[derive(Debug, Clone)]
pub enum Node {
    Program(Vec<Node>),
    FunctionDeclaration { name: String, params: Vec<String>, body: Box<Node>, is_async: bool },
    ArrowFunction { params: Vec<String>, body: Box<Node> },
    ClassDeclaration { name: String, superclass: Option<String>, methods: Vec<Node> },
    MethodDefinition { name: String, params: Vec<String>, body: Box<Node>, kind: String, is_static: bool },
    VariableDeclaration { kind: String, name: String, init: Option<Box<Node>> },
    IfStatement { condition: Box<Node>, consequent: Box<Node>, alternate: Option<Box<Node>> },
    WhileStatement { condition: Box<Node>, body: Box<Node> },
    ForStatement { init: Option<Box<Node>>, condition: Option<Box<Node>>, update: Option<Box<Node>>, body: Box<Node> },
    ForInStatement { left: Box<Node>, right: Box<Node>, body: Box<Node>, is_await: bool },
    ForOfStatement { left: Box<Node>, right: Box<Node>, body: Box<Node>, is_await: bool },
    SwitchStatement { discriminant: Box<Node>, cases: Vec<(Option<Node>, Vec<Node>)> },
    TryStatement { block: Box<Node>, param: Option<String>, handler: Option<Box<Node>>, finalizer: Option<Box<Node>> },
    ThrowStatement { argument: Box<Node> },
    ReturnStatement { argument: Option<Box<Node>> },
    BreakStatement { label: Option<String> },
    ContinueStatement { label: Option<String> },
    BlockStatement { body: Vec<Node> },
    ExpressionStatement { expression: Box<Node> },
    AssignmentExpression { left: Box<Node>, right: Box<Node>, operator: String },
    BinaryExpression { left: Box<Node>, right: Box<Node>, operator: String },
    UnaryExpression { operator: String, argument: Box<Node>, prefix: bool },
    LogicalExpression { left: Box<Node>, right: Box<Node>, operator: String },
    ConditionalExpression { test: Box<Node>, consequent: Box<Node>, alternate: Box<Node> },
    CallExpression { callee: Box<Node>, arguments: Vec<Node> },
    NewExpression { callee: Box<Node>, arguments: Vec<Node> },
    MemberExpression { object: Box<Node>, property: Box<Node>, computed: bool, optional: bool },
    OptionalCallExpression { callee: Box<Node>, arguments: Vec<Node> },
    ArrayExpression { elements: Vec<Option<Node>> },
    ObjectExpression { properties: Vec<(Node, Node)> },
    SequenceExpression { expressions: Vec<Node> },
    SpreadElement { argument: Box<Node> },
    Identifier { name: String },
    Literal { value: Value },
    TemplateLiteral { quasis: Vec<String>, expressions: Vec<Node> },
    TaggedTemplateExpression { tag: Box<Node>, template: Box<Node> },
    AwaitExpression { argument: Box<Node> },
    YieldExpression { argument: Option<Box<Node>>, delegate: bool },
    ThisExpression,
    Super,
    ImportDeclaration { specifiers: Vec<(String, Option<String>)>, source: String },
    ExportNamedDeclaration { specifiers: Vec<(String, Option<String>)>, source: Option<String> },
    ExportDefaultDeclaration { declaration: Box<Node> },
    EmptyStatement,
    LabelledStatement { label: String, body: Box<Node> },
    DebuggerStatement,
}

#[derive(Debug, Clone)]
pub enum Value {
    Number(f64),
    String(String),
    Boolean(bool),
    Null,
    Undefined,
}

pub struct Parser<'a> {
    tokens: Vec<TokenWithPosition>,
    pos: usize,
    source: &'a str,
}

impl<'a> Parser<'a> {
    pub fn new(tokens: Vec<TokenWithPosition>, source: &'a str) -> Self {
        Parser { tokens, pos: 0, source }
    }

    fn current(&self) -> &Token {
        &self.tokens[self.pos].token
    }

    fn peek(&self, offset: usize) -> &Token {
        if self.pos + offset < self.tokens.len() {
            &self.tokens[self.pos + offset].token
        } else {
            &Token::EndOfFile
        }
    }

    fn advance(&mut self) -> TokenWithPosition {
        let t = self.tokens[self.pos].clone();
        self.pos += 1;
        t
    }

    fn expect(&mut self, token: Token) -> Result<TokenWithPosition, String> {
        if self.current() == &token {
            Ok(self.advance())
        } else {
            Err(format!("Expected {:?}, got {:?}", token, self.current()))
        }
    }

    fn match_token(&mut self, token: &Token) -> bool {
        if self.current() == token {
            self.advance();
            true
        } else {
            false
        }
    }

    pub fn parse(&mut self) -> Result<Node, String> {
        let mut body = Vec::new();

        while self.current() != &Token::EndOfFile {
            match self.parse_module_item() {
                Ok(node) => body.push(node),
                Err(e) => return Err(e),
            }
        }

        Ok(Node::Program(body))
    }

    fn parse_module_item(&mut self) -> Result<Node, String> {
        if let Token::Import = self.current() {
            return self.parse_import_declaration();
        }
        if let Token::Export = self.current() {
            return self.parse_export_declaration();
        }
        self.parse_statement()
    }

    fn parse_import_declaration(&mut self) -> Result<Node, String> {
        self.advance();
        let mut specifiers = Vec::new();

        if self.match_token(&Token::LeftBrace) {
            while !self.match_token(&Token::RightBrace) {
                let name = match self.current() {
                    Token::Identifier(n) => n.clone(),
                    _ => return Err("Expected identifier".to_string()),
                };
                self.advance();

                let alias = if self.match_token(&Token::As) {
                    match self.current() {
                        Token::Identifier(n) => {
                            let n = n.clone();
                            self.advance();
                            Some(n)
                        }
                        _ => return Err("Expected identifier after 'as'".to_string()),
                    }
                } else {
                    None
                };

                specifiers.push((name, alias));
                self.match_token(&Token::Comma);
            }
            self.expect(Token::From)?;
        } else if self.match_token(&Token::Identifier(String::new())) {
            let name = match self.current() {
                Token::Identifier(n) => n.clone(),
                _ => return Err("Expected identifier".to_string()),
            };
            self.advance();
            specifiers.push((name, None));
        }

        match self.current() {
            Token::String(s) => {
                let source = s.clone();
                self.advance();
                self.match_token(&Token::SemiColon);
                Ok(Node::ImportDeclaration { specifiers, source })
            }
            _ => Err("Expected string after import".to_string()),
        }
    }

    fn parse_export_declaration(&mut self) -> Result<Node, String> {
        self.advance();

        if self.match_token(&Token::Default) {
            let decl = self.parse_expression()?;
            self.match_token(&Token::SemiColon);
            return Ok(Node::ExportDefaultDeclaration { declaration: Box::new(decl) });
        }

        if self.match_token(&Token::LeftBrace) {
            let mut specifiers = Vec::new();
            while !self.match_token(&Token::RightBrace) {
                let name = match self.current() {
                    Token::Identifier(n) => n.clone(),
                    _ => return Err("Expected identifier".to_string()),
                };
                self.advance();

                let alias = if self.match_token(&Token::As) {
                    match self.current() {
                        Token::Identifier(n) => {
                            let n = n.clone();
                            self.advance();
                            Some(n)
                        }
                        _ => return Err("Expected identifier".to_string()),
                    }
                } else {
                    None
                };

                specifiers.push((name, alias));
                self.match_token(&Token::Comma);
            }

            let source = if self.match_token(&Token::From) {
                match self.current() {
                    Token::String(s) => {
                        let s = s.clone();
                        self.advance();
                        Some(s)
                    }
                    _ => return Err("Expected string".to_string()),
                }
            } else {
                None
            };

            self.match_token(&Token::SemiColon);
            return Ok(Node::ExportNamedDeclaration { specifiers, source });
        }

        let decl = self.parse_statement()?;
        self.match_token(&Token::SemiColon);
        Ok(Node::ExportDefaultDeclaration { declaration: Box::new(decl) })
    }

    fn parse_statement(&mut self) -> Result<Node, String> {
        match self.current() {
            Token::LeftBrace => {
                self.advance();
                let mut body = Vec::new();
                while !self.match_token(&Token::RightBrace) {
                    body.push(self.parse_statement()?);
                }
                Ok(Node::BlockStatement { body })
            },
            Token::Var | Token::Let | Token::Const => {
                let kind = match self.current() {
                    Token::Var => "var",
                    Token::Let => "let",
                    Token::Const => "const",
                    _ => "let",
                };
                self.advance();

                let name = match self.current() {
                    Token::Identifier(n) => n.clone(),
                    _ => return Err("Expected identifier".to_string()),
                };
                self.advance();

                let init = if self.match_token(&Token::Equal) {
                    Some(Box::new(self.parse_expression()?))
                } else {
                    None
                };

                self.match_token(&Token::SemiColon);
                Ok(Node::VariableDeclaration { kind: kind.to_string(), name, init })
            },
            Token::If => {
                self.advance();
                self.expect(Token::LeftParen)?;
                let condition = Box::new(self.parse_expression()?);
                self.expect(Token::RightParen)?;
                let consequent = Box::new(self.parse_statement()?);
                let alternate = if self.match_token(&Token::Else) {
                    Some(Box::new(self.parse_statement()?))
                } else {
                    None
                };
                Ok(Node::IfStatement { condition, consequent, alternate })
            },
            Token::While => {
                self.advance();
                self.expect(Token::LeftParen)?;
                let condition = Box::new(self.parse_expression()?);
                self.expect(Token::RightParen)?;
                let body = Box::new(self.parse_statement()?);
                Ok(Node::WhileStatement { condition, body })
            },
            Token::For => {
                self.advance();
                self.expect(Token::LeftParen)?;

                let init = if self.match_token(&Token::SemiColon) {
                    None
                } else if self.match_token(&Token::Var) || self.match_token(&Token::Let) || self.match_token(&Token::Const) {
                    let name = match self.current() {
                        Token::Identifier(n) => n.clone(),
                        _ => return Err("Expected identifier".to_string()),
                    };
                    self.advance();
                    let init = if self.match_token(&Token::Equal) {
                        Some(Box::new(self.parse_expression()?))
                    } else {
                        None
                    };
                    self.match_token(&Token::SemiColon);
                    Some(Box::new(Node::VariableDeclaration {
                        kind: "let".to_string(),
                        name,
                        init,
                    }))
                } else {
                    let expr = self.parse_expression()?;
                    self.match_token(&Token::SemiColon);
                    Some(Box::new(expr))
                };

                let condition = if !self.match_token(&Token::SemiColon) {
                    Some(Box::new(self.parse_expression()?))
                } else {
                    None
                };

                self.match_token(&Token::SemiColon);

                let update = if !self.match_token(&Token::RightParen) {
                    Some(Box::new(self.parse_expression()?))
                } else {
                    None
                };

                self.expect(Token::RightParen)?;
                let body = Box::new(self.parse_statement()?);

                Ok(Node::ForStatement { init, condition, update, body })
            },
            Token::Return => {
                self.advance();
                let argument = if !self.match_token(&Token::SemiColon) && self.current() != &Token::RightBrace && self.current() != &Token::EndOfFile {
                    Some(Box::new(self.parse_expression()?))
                } else {
                    None
                };
                self.match_token(&Token::SemiColon);
                Ok(Node::ReturnStatement { argument })
            },
            Token::Throw => {
                self.advance();
                let argument = Box::new(self.parse_expression()?);
                self.match_token(&Token::SemiColon);
                Ok(Node::ThrowStatement { argument })
            },
            Token::Break => {
                self.advance();
                self.match_token(&Token::SemiColon);
                Ok(Node::BreakStatement { label: None })
            },
            Token::Continue => {
                self.advance();
                self.match_token(&Token::SemiColon);
                Ok(Node::ContinueStatement { label: None })
            },
            Token::Try => {
                self.advance();
                let block = Box::new(self.parse_statement()?);

                let param = if self.match_token(&Token::Catch) {
                    self.expect(Token::LeftParen)?;
                    let name = match self.current() {
                        Token::Identifier(n) => n.clone(),
                        _ => return Err("Expected identifier".to_string()),
                    };
                    self.advance();
                    self.expect(Token::RightParen)?;
                    Some(name)
                } else {
                    None
                };

                let handler = if self.match_token(&Token::Catch) || param.is_some() {
                    Some(Box::new(self.parse_statement()?))
                } else {
                    None
                };

                let finalizer = if self.match_token(&Token::Finally) {
                    Some(Box::new(self.parse_statement()?))
                } else {
                    None
                };

                Ok(Node::TryStatement { block, param, handler, finalizer })
            },
            Token::Switch => {
                self.advance();
                self.expect(Token::LeftParen)?;
                let discriminant = Box::new(self.parse_expression()?);
                self.expect(Token::RightParen)?;
                self.expect(Token::LeftBrace)?;

                let mut cases = Vec::new();
                while !self.match_token(&Token::RightBrace) {
                    if self.match_token(&Token::Case) {
                        let test = Some(self.parse_expression()?);
                        self.expect(Token::Colon)?;
                        let mut consequent = Vec::new();
                        while !self.match_token(&Token::Case) &&
                              !self.match_token(&Token::Default) &&
                              self.current() != &Token::RightBrace {
                            consequent.push(self.parse_statement()?);
                        }
                        cases.push((test, consequent));
                    } else if self.match_token(&Token::Default) {
                        self.expect(Token::Colon)?;
                        let mut consequent = Vec::new();
                        while !self.match_token(&Token::Case) && self.current() != &Token::RightBrace {
                            consequent.push(self.parse_statement()?);
                        }
                        cases.push((None, consequent));
                    }
                }

                Ok(Node::SwitchStatement { discriminant, cases })
            },
            Token::Function => {
                self.advance();
                let is_async = false;
                let name = match self.current() {
                    Token::Identifier(n) => n.clone(),
                    _ => return Err("Expected function name".to_string()),
                };
                self.advance();

                self.expect(Token::LeftParen)?;
                let mut params = Vec::new();
                while !self.match_token(&Token::RightParen) {
                    match self.current() {
                        Token::Identifier(n) => params.push(n.clone()),
                        Token::DotDotDot => {
                            self.advance();
                            if let Token::Identifier(n) = self.current() {
                                params.push(format!("...{}", n));
                                self.advance();
                            }
                        }
                        _ => return Err("Expected parameter".to_string()),
                    }
                    self.match_token(&Token::Comma);
                }

                let body = Box::new(self.parse_statement()?);
                Ok(Node::FunctionDeclaration { name, params, body, is_async })
            },
            Token::Async => {
                self.advance();
                if self.match_token(&Token::Fun) {
                    let is_async = true;
                    let name = match self.current() {
                        Token::Identifier(n) => n.clone(),
                        _ => "anonymous".to_string(),
                    };
                    if !self.match_token(&Token::Identifier(String::new())) {
                        self.advance();
                    }

                    self.expect(Token::LeftParen)?;
                    let mut params = Vec::new();
                    while !self.match_token(&Token::RightParen) {
                        match self.current() {
                            Token::Identifier(n) => params.push(n.clone()),
                            _ => return Err("Expected parameter".to_string()),
                        }
                        self.match_token(&Token::Comma);
                    }

                    let body = Box::new(self.parse_statement()?);
                    Ok(Node::FunctionDeclaration { name, params, body, is_async })
                } else {
                    self.parse_expression()
                }
            },
            Token::Class => {
                self.advance();
                let name = match self.current() {
                    Token::Identifier(n) => n.clone(),
                    _ => return Err("Expected class name".to_string()),
                };
                self.advance();

                let superclass = if self.match_token(&Token::Extends) {
                    match self.current() {
                        Token::Identifier(n) => {
                            let n = n.clone();
                            self.advance();
                            Some(n)
                        }
                        _ => return Err("Expected superclass name".to_string()),
                    }
                } else {
                    None
                };

                self.expect(Token::LeftBrace)?;

                let mut methods = Vec::new();
                while !self.match_token(&Token::RightBrace) {
                    let is_static = self.match_token(&Token::Static);
                    let kind = if self.match_token(&Token::Get) {
                        "get"
                    } else if self.match_token(&Token::Set) {
                        "set"
                    } else {
                        "method"
                    };

                    let name = match self.current() {
                        Token::Identifier(n) => n.clone(),
                        _ => return Err("Expected method name".to_string()),
                    };
                    self.advance();

                    self.expect(Token::LeftParen)?;
                    let mut params = Vec::new();
                    while !self.match_token(&Token::RightParen) {
                        match self.current() {
                            Token::Identifier(n) => params.push(n.clone()),
                            _ => return Err("Expected parameter".to_string()),
                        }
                        self.match_token(&Token::Comma);
                    }

                    let body = Box::new(self.parse_statement()?);
                    methods.push(Node::MethodDefinition {
                        name,
                        params,
                        body,
                        kind: kind.to_string(),
                        is_static,
                    });
                }

                Ok(Node::ClassDeclaration { name, superclass, methods })
            },
            Token::SemiColon => {
                self.advance();
                Ok(Node::EmptyStatement)
            },
            _ => {
                let expr = self.parse_expression()?;
                self.match_token(&Token::SemiColon);
                Ok(Node::ExpressionStatement { expression: Box::new(expr) })
            },
        }
    }

    fn parse_expression(&mut self) -> Result<Node, String> {
        self.parse_assignment()
    }

    fn parse_assignment(&mut self) -> Result<Node, String> {
        let left = self.parse_conditional()?;

        if let Token::Equal = self.current().clone() {
            self.advance();
            let right = self.parse_assignment()?;
            return Ok(Node::AssignmentExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: "=".to_string(),
            });
        }

        let compound_ops = [
            Token::PlusEqual,
            Token::MinusEqual,
            Token::StarEqual,
            Token::SlashEqual,
            Token::PercentEqual,
            Token::StarStarEqual,
            Token::AmpEqual,
            Token::PipeEqual,
            Token::CaretEqual,
            Token::QuestionQuestionEqual,
        ];

        for op in compound_ops {
            if self.current() == &op {
                self.advance();
                let right = self.parse_assignment()?;
                return Ok(Node::AssignmentExpression {
                    left: Box::new(left),
                    right: Box::new(right),
                    operator: format!("{:?}", op).replace("Equal", "=").to_lowercase(),
                });
            }
        }

        Ok(left)
    }

    fn parse_conditional(&mut self) -> Result<Node, String> {
        let test = self.parse_or()?;

        if self.match_token(&Token::QuestionMark) {
            let consequent = Box::new(self.parse_assignment()?);
            self.expect(Token::Colon)?;
            let alternate = Box::new(self.parse_assignment()?);
            return Ok(Node::ConditionalExpression { test: Box::new(test), consequent, alternate });
        }

        Ok(test)
    }

    fn parse_or(&mut self) -> Result<Node, String> {
        let mut left = self.parse_and()?;

        while self.match_token(&Token::PipePipe) {
            let right = self.parse_and()?;
            left = Node::LogicalExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: "||".to_string(),
            };
        }

        Ok(left)
    }

    fn parse_and(&mut self) -> Result<Node, String> {
        let mut left = self.parse_nullish()?;

        while self.match_token(&Token::AmpAmp) {
            let right = self.parse_nullish()?;
            left = Node::LogicalExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: "&&".to_string(),
            };
        }

        Ok(left)
    }

    fn parse_nullish(&mut self) -> Result<Node, String> {
        let mut left = self.parse_bitwise_or()?;

        while self.match_token(&Token::QuestionQuestion) {
            let right = self.parse_bitwise_or()?;
            left = Node::LogicalExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: "??".to_string(),
            };
        }

        Ok(left)
    }

    fn parse_bitwise_or(&mut self) -> Result<Node, String> {
        let mut left = self.parse_bitwise_xor()?;

        while self.match_token(&Token::Pipe) {
            let right = self.parse_bitwise_xor()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: "|".to_string(),
            };
        }

        Ok(left)
    }

    fn parse_bitwise_xor(&mut self) -> Result<Node, String> {
        let mut left = self.parse_bitwise_and()?;

        while self.match_token(&Token::Caret) {
            let right = self.parse_bitwise_and()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: "^".to_string(),
            };
        }

        Ok(left)
    }

    fn parse_bitwise_and(&mut self) -> Result<Node, String> {
        let mut left = self.parse_equality()?;

        while self.match_token(&Token::Amp) {
            let right = self.parse_equality()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: "&".to_string(),
            };
        }

        Ok(left)
    }

    fn parse_equality(&mut self) -> Result<Node, String> {
        let mut left = self.parse_comparison()?;

        let eq_ops = [
            Token::EqualEqual,
            Token::EqualEqualEqual,
            Token::BangEqual,
            Token::BangEqualEqual,
        ];

        while let Some(op) = eq_ops.iter().find(|op| self.current() == **op) {
            self.advance();
            let right = self.parse_comparison()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: format!("{:?}", op).to_lowercase(),
            };
        }

        Ok(left)
    }

    fn parse_comparison(&mut self) -> Result<Node, String> {
        let mut left = self.parse_shift()?;

        let comp_ops = [
            Token::Greater,
            Token::GreaterEqual,
            Token::Less,
            Token::LessEqual,
            Token::Instanceof,
            Token::In,
        ];

        while let Some(op) = comp_ops.iter().find(|op| self.current() == **op) {
            self.advance();
            let right = self.parse_shift()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: format!("{:?}", op).to_lowercase(),
            };
        }

        Ok(left)
    }

    fn parse_shift(&mut self) -> Result<Node, String> {
        let mut left = self.parse_additive()?;

        let shift_ops = [
            Token::Greater,
            Token::Less,
        ];

        while let Some(op) = shift_ops.iter().find(|op| self.current() == **op) {
            self.advance();
            let right = self.parse_additive()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: format!("{:?}", op).to_lowercase(),
            };
        }

        Ok(left)
    }

    fn parse_additive(&mut self) -> Result<Node, String> {
        let mut left = self.parse_multiplicative()?;

        while self.match_token(&Token::Plus) || self.match_token(&Token::Minus) {
            let op = if self.current() == &Token::Plus { "+" } else { "-" }.to_string();
            let right = self.parse_multiplicative()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: op,
            };
        }

        Ok(left)
    }

    fn parse_multiplicative(&mut self) -> Result<Node, String> {
        let mut left = self.parse_unary()?;

        let mult_ops = [Token::Star, Token::Slash, Token::Percent];

        while let Some(op) = mult_ops.iter().find(|op| self.current() == **op) {
            self.advance();
            let right = self.parse_unary()?;
            left = Node::BinaryExpression {
                left: Box::new(left),
                right: Box::new(right),
                operator: format!("{:?}", op).to_lowercase(),
            };
        }

        Ok(left)
    }

    fn parse_unary(&mut self) -> Result<Node, String> {
        let prefix_ops = [
            (Token::Bang, "!"),
            (Token::Minus, "-"),
            (Token::Plus, "+"),
            (Token::PlusPlus, "++"),
            (Token::MinusMinus, "--"),
            (Token::Typeof, "typeof"),
            (Token::Void, "void"),
            (Token::Delete, "delete"),
            (Token::Await, "await"),
            (Token::Yield, "yield"),
        ];

        for (token, op) in prefix_ops {
            if self.current() == &token {
                self.advance();
                let argument = Box::new(self.parse_unary()?);
                return Ok(Node::UnaryExpression { operator: op.to_string(), argument, prefix: true });
            }
        }

        self.parse_postfix()
    }

    fn parse_postfix(&mut self) -> Result<Node, String> {
        let mut expr = self.parse_left_hand_side()?;

        while self.match_token(&Token::PlusPlus) || self.match_token(&Token::MinusMinus) {
            let op = if self.current() == &Token::PlusPlus { "++" } else { "--" }.to_string();
            expr = Node::UnaryExpression {
                operator: op,
                argument: Box::new(expr),
                prefix: false,
            };
        }

        Ok(expr)
    }

    fn parse_left_hand_side(&mut self) -> Result<Node, String> {
        let expr = self.parse_call_member()?;

        if self.match_token(&Token::QuestionDot) {
            let property = Box::new(self.parse_identifier()?);
            return Ok(Node::MemberExpression {
                object: Box::new(expr),
                property,
                computed: false,
                optional: true,
            });
        }

        Ok(expr)
    }

    fn parse_call_member(&mut self) -> Result<Node, String> {
        let mut expr = self.parse_primary()?;

        loop {
            if self.match_token(&Token::LeftParen) {
                let mut args = Vec::new();
                while !self.match_token(&Token::RightParen) {
                    args.push(self.parse_assignment()?);
                    self.match_token(&Token::Comma);
                }
                expr = Node::CallExpression { callee: Box::new(expr), arguments: args };
            } else if self.match_token(&Token::Dot) {
                let property = Box::new(self.parse_identifier()?);
                expr = Node::MemberExpression { object: Box::new(expr), property, computed: false, optional: false };
            } else if self.match_token(&Token::LeftBracket) {
                let property = Box::new(self.parse_expression()?);
                self.expect(Token::RightBracket)?;
                expr = Node::MemberExpression { object: Box::new(expr), property, computed: true, optional: false };
            } else if self.match_token(&Token::QuestionDot) {
                if self.match_token(&Token::LeftParen) {
                    let mut args = Vec::new();
                    while !self.match_token(&Token::RightParen) {
                        args.push(self.parse_assignment()?);
                        self.match_token(&Token::Comma);
                    }
                    expr = Node::OptionalCallExpression { callee: Box::new(expr), arguments: args };
                } else {
                    let property = Box::new(self.parse_identifier()?);
                    expr = Node::MemberExpression { object: Box::new(expr), property, computed: false, optional: true };
                }
            } else {
                break;
            }
        }

        Ok(expr)
    }

    fn parse_primary(&mut self) -> Result<Node, String> {
        match self.current() {
            Token::True => {
                self.advance();
                Ok(Node::Literal { value: Value::Boolean(true) })
            },
            Token::False => {
                self.advance();
                Ok(Node::Literal { value: Value::Boolean(false) })
            },
            Token::Null => {
                self.advance();
                Ok(Node::Literal { value: Value::Null })
            },
            Token::Undefined => {
                self.advance();
                Ok(Node::Literal { value: Value::Undefined })
            },
            Token::Number(n) => {
                self.advance();
                Ok(Node::Literal { value: Value::Number(*n) })
            },
            Token::String(s) => {
                self.advance();
                Ok(Node::Literal { value: Value::String(s.clone()) })
            },
            Token::TemplateString(s) => {
                self.advance();
                Ok(Node::TemplateLiteral {
                    quasis: vec![s.clone()],
                    expressions: vec![],
                })
            },
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();

                if self.match_token(&Token::Arrow) {
                    let body = if self.match_token(&Token::LeftBrace) {
                        let mut body = Vec::new();
                        while !self.match_token(&Token::RightBrace) {
                            body.push(self.parse_statement()?);
                        }
                        Node::BlockStatement { body }
                    } else {
                        self.parse_expression()?
                    };
                    return Ok(Node::ArrowFunction { params: vec![name], body: Box::new(body) });
                }

                Ok(Node::Identifier { name })
            },
            Token::This => {
                self.advance();
                Ok(Node::ThisExpression)
            },
            Token::Super => {
                self.advance();
                Ok(Node::Super)
            },
            Token::New => {
                self.advance();
                let callee = Box::new(self.parse_left_hand_side()?);
                self.expect(Token::LeftParen)?;
                let mut args = Vec::new();
                while !self.match_token(&Token::RightParen) {
                    args.push(self.parse_assignment()?);
                    self.match_token(&Token::Comma);
                }
                Ok(Node::NewExpression { callee, arguments: args })
            },
            Token::Function => {
                self.advance();
                let name = match self.current() {
                    Token::Identifier(n) => {
                        let n = n.clone();
                        self.advance();
                        n
                    }
                    _ => "anonymous".to_string(),
                };

                self.expect(Token::LeftParen)?;
                let mut params = Vec::new();
                while !self.match_token(&Token::RightParen) {
                    match self.current() {
                        Token::Identifier(n) => params.push(n.clone()),
                        Token::DotDotDot => {
                            self.advance();
                            if let Token::Identifier(n) = self.current() {
                                params.push(format!("...{}", n));
                                self.advance();
                            }
                        }
                        _ => return Err("Expected parameter".to_string()),
                    }
                    self.match_token(&Token::Comma);
                }

                let body = Box::new(self.parse_statement()?);
                Ok(Node::FunctionDeclaration { name, params, body, is_async: false })
            },
            Token::LeftParen => {
                self.advance();

                if self.match_token(&Token::RightParen) {
                    self.expect(Token::Arrow)?;
                    return self.parse_arrow_body(vec![]);
                }

                let first = match self.current() {
                    Token::Identifier(n) => n.clone(),
                    _ => return Err("Expected identifier".to_string()),
                };
                self.advance();

                if self.match_token(&Token::RightParen) {
                    self.expect(Token::Arrow)?;
                    return self.parse_arrow_body(vec![first]);
                }

                self.match_token(&Token::Comma);
                let mut params = vec![first];

                while !self.match_token(&Token::RightParen) {
                    match self.current() {
                        Token::Identifier(n) => params.push(n.clone()),
                        _ => return Err("Expected parameter".to_string()),
                    }
                    self.advance();
                    self.match_token(&Token::Comma);
                }

                self.expect(Token::Arrow)?;
                self.parse_arrow_body(params)
            },
            Token::LeftBracket => {
                self.advance();
                let mut elements = Vec::new();

                while !self.match_token(&Token::RightBracket) {
                    if self.match_token(&Token::Comma) {
                        elements.push(None);
                    } else {
                        elements.push(Some(self.parse_assignment()?));
                        self.match_token(&Token::Comma);
                    }
                }

                Ok(Node::ArrayExpression { elements })
            },
            Token::LeftBrace => {
                self.advance();
                let mut properties = Vec::new();

                while !self.match_token(&Token::RightBrace) {
                    let key = match self.current() {
                        Token::Identifier(n) => {
                            let n = n.clone();
                            self.advance();
                            Node::Identifier { name: n }
                        },
                        Token::String(s) => {
                            let s = s.clone();
                            self.advance();
                            Node::Literal { value: Value::String(s) }
                        },
                        Token::Number(n) => {
                            let n = *n;
                            self.advance();
                            Node::Literal { value: Value::Number(n) }
                        },
                        _ => return Err("Expected property key".to_string()),
                    };

                    self.expect(Token::Colon)?;
                    let value = self.parse_assignment()?;

                    properties.push((key, value));
                    self.match_token(&Token::Comma);
                }

                Ok(Node::ObjectExpression { properties })
            },
            Token::Arrow => {
                self.advance();
                self.parse_arrow_body(vec![])
            },
            Token::Async => {
                self.advance();
                if self.match_token(&Token::Fun) {
                    let name = match self.current() {
                        Token::Identifier(n) => {
                            let n = n.clone();
                            self.advance();
                            n
                        }
                        _ => "anonymous".to_string(),
                    };

                    self.expect(Token::LeftParen)?;
                    let mut params = Vec::new();
                    while !self.match_token(&Token::RightParen) {
                        match self.current() {
                            Token::Identifier(n) => params.push(n.clone()),
                            _ => return Err("Expected parameter".to_string()),
                        }
                        self.advance();
                        self.match_token(&Token::Comma);
                    }

                    let body = Box::new(self.parse_statement()?);
                    return Ok(Node::FunctionDeclaration { name, params, body, is_async: true });
                }

                if self.match_token(&Token::LeftParen) {
                    let mut params = Vec::new();
                    while !self.match_token(&Token::RightParen) {
                        match self.current() {
                            Token::Identifier(n) => params.push(n.clone()),
                            _ => return Err("Expected parameter".to_string()),
                        }
                        self.advance();
                        self.match_token(&Token::Comma);
                    }
                    self.expect(Token::Arrow)?;
                    return self.parse_arrow_body(params);
                }

                if let Token::Identifier(n) = self.current() {
                    let n = n.clone();
                    self.advance();
                    self.expect(Token::Arrow)?;
                    return self.parse_arrow_body(vec![n]);
                }

                Err("Expected async function or arrow function".to_string())
            },
            _ => Err(format!("Unexpected token: {:?}", self.current())),
        }
    }

    fn parse_identifier(&mut self) -> Result<Node, String> {
        match self.current() {
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();
                Ok(Node::Identifier { name })
            },
            _ => Err("Expected identifier".to_string()),
        }
    }

    fn parse_arrow_body(&mut self, params: Vec<String>) -> Result<Node, String> {
        if self.match_token(&Token::LeftBrace) {
            let mut body = Vec::new();
            while !self.match_token(&Token::RightBrace) {
                body.push(self.parse_statement()?);
            }
            Ok(Node::ArrowFunction { params, body: Box::new(Node::BlockStatement { body }) })
        } else {
            let expr = self.parse_expression()?;
            Ok(Node::ArrowFunction { params, body: Box::new(expr) })
        }
    }
}

pub fn parse(source: &str) -> Result<Node, String> {
    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize();
    let mut parser = Parser::new(tokens, source);
    parser.parse()
}

pub fn tokenize(source: &str) -> Vec<TokenWithPosition> {
    let mut lexer = Lexer::new(source);
    lexer.tokenize()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_simple() {
        let result = parse("let x = 1;");
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_function() {
        let result = parse("function foo(x, y) { return x + y; }");
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_arrow() {
        let result = parse("const add = (x, y) => x + y;");
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_class() {
        let result = parse("class Foo { constructor() {} method() {} }");
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_async() {
        let result = parse("async function foo() { await fetch(); }");
        assert!(result.is_ok());
    }
}