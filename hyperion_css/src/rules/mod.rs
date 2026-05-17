// ============================================================
// Hyperion CSS Parser — Rules
// ============================================================

use crate::selectors::SelectorList;
use crate::declarations::Declaration;

#[derive(Debug, Clone, PartialEq)]
pub enum Rule {
    StyleRule {
        selectors: SelectorList,
        declarations: Vec<Declaration>,
    },
    AtRule {
        name: String,
        prelude: String,
        block: Option<Vec<Rule>>,
    },
}
