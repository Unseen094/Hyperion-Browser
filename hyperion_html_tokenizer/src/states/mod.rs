// ============================================================
// Hyperion HTML Tokenizer — Tokenizer States
//
// Each variant of `TokenizerState` maps to a dedicated handler
// in the tokenizer. State transitions are explicit and recorded.
//
// Spec reference: https://html.spec.whatwg.org/multipage/parsing.html
// ============================================================

/// The complete set of tokenizer states.
///
/// Only the states required for the current milestone are active.
/// New states are added here when the corresponding handler is
/// implemented — never before.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TokenizerState {
    // ---- Core states (Phase 1) ----

    /// Default state. Processes text content and recognises `<`.
    Data,

    /// Entered after `<`. Decides whether we're opening a start tag,
    /// end tag, comment, or DOCTYPE.
    TagOpen,

    /// Entered after `</`. Reads the end tag name.
    EndTagOpen,

    /// Consuming a tag name character by character.
    TagName,

    /// Between attributes. Handles whitespace, `>`, `/>`.
    BeforeAttributeName,

    /// Consuming an attribute name.
    AttributeName,

    /// After the attribute name. Expects `=`, `>`, or next attribute.
    AfterAttributeName,

    /// After `=` in an attribute. Determines quote style.
    BeforeAttributeValue,

    /// Inside `"…"` attribute value.
    AttributeValueDoubleQuoted,

    /// Inside `'…'` attribute value.
    AttributeValueSingleQuoted,

    /// Inside an unquoted attribute value.
    AttributeValueUnquoted,

    /// After a quoted attribute value, before the next attribute or `>`.
    AfterAttributeValueQuoted,

    /// Consumed `<` then `/` — reading the end-tag name.
    SelfClosingStartTag,

    // ---- Comment states ----

    /// After `<!`. Expects `-` for comment, or `D`/`d` for DOCTYPE.
    MarkupDeclarationOpen,

    /// After `<!--`. Collecting comment data.
    Comment,

    /// Inside comment body, saw first `-`.
    CommentEndDash,

    /// Inside comment body, saw `--`. Expecting `>`.
    CommentEnd,

    // ---- DOCTYPE states ----

    /// After `<!DOCTYPE`. Consuming name.
    Doctype,

    /// Consuming the DOCTYPE name token.
    DoctypeName,

    // ---- Terminal ----

    /// Emitted an EOF token; processing is complete.
    Eof,

    // ---- Script Data ----
    ScriptData,
    ScriptDataLessThanSign,
    ScriptDataEndTagOpen,
    ScriptDataEndTagName,
}
