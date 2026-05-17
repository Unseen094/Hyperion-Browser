// ============================================================
// Hyperion CSS Parser — Declarations
// ============================================================

use crate::values::CssValue;

#[derive(Debug, Clone, PartialEq)]
pub struct Declaration {
    pub property: String,
    pub value: CssValue,
    pub important: bool,
}

impl Declaration {
    pub fn new(property: String, value: CssValue, important: bool) -> Self {
        Self { property, value, important }
    }
}
