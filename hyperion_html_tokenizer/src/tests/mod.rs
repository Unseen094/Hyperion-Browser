#[cfg(test)]
mod tests {
    use crate::{tokenize, Token};

    fn first_token(html: &str) -> Token {
        let (tokens, _) = tokenize(html);
        tokens.into_iter().find(|t| !t.is_eof()).expect("no non-EOF token")
    }

    fn non_eof_tokens(html: &str) -> Vec<Token> {
        let (tokens, _) = tokenize(html);
        tokens.into_iter().filter(|t| !t.is_eof()).collect()
    }

    fn names(tokens: &[Token]) -> Vec<String> {
        tokens
            .iter()
            .map(|t| match t {
                Token::StartTag { name, .. } => format!("S:{name}"),
                Token::EndTag { name, .. } => format!("E:{name}"),
                Token::Text { data, .. } => format!("T:{data}"),
                Token::Comment { data, .. } => format!("C:{data}"),
                Token::Doctype { data, .. } => format!("D:{}", data.name.as_deref().unwrap_or("?")),
                Token::Eof { .. } => "EOF".into(),
            })
            .collect()
    }

    // ---- Basic structure ----

    #[test]
    fn simple_document() {
        let html = "<html><body>Hello</body></html>";
        let toks = non_eof_tokens(html);
        assert_eq!(names(&toks), ["S:html", "S:body", "T:Hello", "E:body", "E:html"]);
    }

    #[test]
    fn doctype() {
        let (tokens, diags) = tokenize("<!DOCTYPE html><html></html>");
        assert!(diags.is_empty());
        match &tokens[0] {
            Token::Doctype { data, .. } => assert_eq!(data.name.as_deref(), Some("html")),
            _ => panic!("Expected Doctype"),
        }
    }

    #[test]
    fn comment_basic() {
        let tok = first_token("<!-- hello world -->");
        match tok {
            Token::Comment { data, .. } => assert_eq!(data, " hello world "),
            _ => panic!("Expected Comment, got {:?}", tok),
        }
    }

    #[test]
    fn self_closing_tag() {
        let tok = first_token("<br/>");
        match tok {
            Token::StartTag { name, self_closing, .. } => {
                assert_eq!(name, "br");
                assert!(self_closing);
            }
            _ => panic!("Expected StartTag"),
        }
    }

    // ---- Attributes ----

    #[test]
    fn double_quoted_attribute() {
        let tok = first_token(r#"<div id="main">"#);
        match tok {
            Token::StartTag { attributes, .. } => {
                assert_eq!(attributes.len(), 1);
                assert_eq!(attributes[0].name, "id");
                assert_eq!(attributes[0].value, "main");
            }
            _ => panic!(),
        }
    }

    #[test]
    fn single_quoted_attribute() {
        let tok = first_token("<a href='#top'>");
        match tok {
            Token::StartTag { attributes, .. } => {
                assert_eq!(attributes[0].value, "#top");
            }
            _ => panic!(),
        }
    }

    #[test]
    fn unquoted_attribute() {
        let tok = first_token("<input type=text>");
        match tok {
            Token::StartTag { attributes, .. } => {
                assert_eq!(attributes[0].name, "type");
                assert_eq!(attributes[0].value, "text");
            }
            _ => panic!(),
        }
    }

    #[test]
    fn boolean_attribute() {
        let tok = first_token("<input disabled>");
        match tok {
            Token::StartTag { attributes, .. } => {
                assert_eq!(attributes[0].name, "disabled");
                assert!(attributes[0].value.is_empty());
            }
            _ => panic!(),
        }
    }

    #[test]
    fn multiple_attributes() {
        let tok = first_token(r#"<a href="/path" class="link" data-x="1">"#);
        match tok {
            Token::StartTag { attributes, .. } => {
                assert_eq!(attributes.len(), 3);
                assert_eq!(attributes[0].name, "href");
                assert_eq!(attributes[1].name, "class");
                assert_eq!(attributes[2].name, "data-x");
            }
            _ => panic!(),
        }
    }

    // ---- Malformed HTML recovery ----

    #[test]
    fn unterminated_tag_no_crash() {
        let (tokens, diags) = tokenize("<div");
        assert!(!tokens.is_empty()); // Eof emitted
        assert!(!diags.is_empty());  // Recorded an error
    }

    #[test]
    fn bare_lt_no_crash() {
        let (tokens, diags) = tokenize("a < b");
        assert!(!diags.is_empty());
        // Text tokens should be emitted for recovery
        let has_text = tokens.iter().any(|t| matches!(t, Token::Text { .. }));
        assert!(has_text);
    }

    #[test]
    fn unterminated_comment_no_crash() {
        let (tokens, diags) = tokenize("<!-- open comment");
        assert!(!diags.is_empty());
        assert!(tokens.iter().any(|t| matches!(t, Token::Comment { .. })));
    }

    #[test]
    fn malformed_doctype_no_crash() {
        let (_tokens, diags) = tokenize("<!DOCTYPE>");
        assert!(!diags.is_empty());
    }

    #[test]
    fn unknown_markup_decl_no_crash() {
        let (_tokens, diags) = tokenize("<!SOMETHING>");
        assert!(!diags.is_empty());
    }

    // ---- UTF-8 ----

    #[test]
    fn unicode_text() {
        let html = "<p>日本語テキスト</p>";
        let toks = non_eof_tokens(html);
        match &toks[1] {
            Token::Text { data, .. } => assert!(data.contains('語')),
            _ => panic!("Expected Text"),
        }
    }

    #[test]
    fn emoji_in_attribute() {
        let tok = first_token(r#"<span title="🚀 launch">"#);
        match tok {
            Token::StartTag { attributes, .. } => {
                assert!(attributes[0].value.contains('🚀'));
            }
            _ => panic!(),
        }
    }

    // ---- Whitespace ----

    #[test]
    fn whitespace_only_text_flagged() {
        let html = "<div>   \n\t  </div>";
        let toks = non_eof_tokens(html);
        match &toks[1] {
            Token::Text { is_whitespace_only, .. } => assert!(*is_whitespace_only),
            _ => panic!("Expected whitespace Text"),
        }
    }

    // ---- Source positions ----

    #[test]
    fn position_tracking_line_col() {
        let html = "<html>\n<body>";
        let toks = non_eof_tokens(html);
        // <body> should be on line 2
        let body = toks.iter().find(|t| matches!(t, Token::StartTag { name, .. } if name == "body"));
        let span = body.expect("no body tag").span();
        assert_eq!(span.start.line, 2);
    }

    // ---- End-to-end ----

    #[test]
    fn nested_elements() {
        let html = "<ul><li>A</li><li>B</li></ul>";
        let toks = non_eof_tokens(html);
        let n = names(&toks);
        assert_eq!(n, ["S:ul", "S:li", "T:A", "E:li", "S:li", "T:B", "E:li", "E:ul"]);
    }

    #[test]
    fn meta_self_close() {
        let tok = first_token(r#"<meta charset="UTF-8"/>"#);
        match tok {
            Token::StartTag { name, self_closing, attributes, .. } => {
                assert_eq!(name, "meta");
                assert!(self_closing);
                assert_eq!(attributes[0].name, "charset");
            }
            _ => panic!(),
        }
    }
}
