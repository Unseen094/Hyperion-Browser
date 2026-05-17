// ============================================================
// Hyperion CSS Parser — Stylesheet
// ============================================================

use crate::rules::Rule;

#[derive(Debug, Clone, Default)]
pub struct Stylesheet {
    pub rules: Vec<Rule>,
}

impl Stylesheet {
    pub fn new() -> Self {
        Self { rules: Vec::new() }
    }
}
