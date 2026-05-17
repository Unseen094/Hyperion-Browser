// ============================================================
// Hyperion CSS Parser — Selectors
// ============================================================

#[derive(Debug, Clone, PartialEq)]
pub enum SimpleSelector {
    TagName(String),
    Id(String),
    Class(String),
    PseudoClass(String),
    Attribute { name: String, operator: Option<String>, value: Option<String> },
}

#[derive(Debug, Clone, PartialEq)]
pub enum Combinator {
    Descendant,       // " "
    Child,            // ">"
    NextSibling,      // "+"
    SubsequentSibling, // "~"
}

#[derive(Debug, Clone, PartialEq)]
pub enum SelectorPart {
    Compound(Vec<SimpleSelector>),
    Combinator(Combinator),
}

#[derive(Debug, Clone, PartialEq)]
pub struct ComplexSelector {
    pub parts: Vec<SelectorPart>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct SelectorList {
    pub selectors: Vec<ComplexSelector>,
}
