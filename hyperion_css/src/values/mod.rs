// ============================================================
// Hyperion CSS Parser — CSS Values
// ============================================================

#[derive(Debug, Clone, PartialEq)]
pub enum CssValue {
    Keyword(String),
    Color(String),       // #hex or rgb()
    Length(f32, String), // 10px, 1.5em
    Percentage(f32),     // 100%
    Number(f32),         // 1, 1.5
    String(String),      // "hello"
    Function(String, Vec<CssValue>),
    List(Vec<CssValue>), // margin: 10px 20px;
}

impl CssValue {
    pub fn to_css_string(&self) -> String {
        match self {
            CssValue::Keyword(k) => k.clone(),
            CssValue::Color(c) => c.clone(),
            CssValue::Length(v, u) => format!("{}{}", v, u),
            CssValue::Percentage(p) => format!("{}%", p),
            CssValue::Number(n) => n.to_string(),
            CssValue::String(s) => format!("\"{}\"", s),
            CssValue::Function(name, args) => {
                let arg_strs: Vec<String> = args.iter().map(|a| a.to_css_string()).collect();
                format!("{}({})", name, arg_strs.join(", "))
            }
            CssValue::List(items) => {
                let strs: Vec<String> = items.iter().map(|i| i.to_css_string()).collect();
                strs.join(" ")
            }
        }
    }
}
