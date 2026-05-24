#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace hre::css {

// Forward declarations
struct CssDeclaration {
    std::string property;
    std::string value;
    bool important;
};

struct CssRule {
    std::vector<std::wstring> selectors;
    std::vector<CssDeclaration> declarations;
};

struct CssStylesheet {
    std::vector<CssRule> rules;
};

// Rust FFI bindings (extern "C" functions from Rust DLL)
// These are declared in the global namespace to match Rust's #[no_mangle]
extern "C" {
    char* rust_parse_css(const char* css_input);
    void rust_free_css_string(char* s);
}

/**
 * High-level CSS Parser interface.
 * Uses the Rust-based lexer/parser for high performance.
 */
class Parser {
public:
    /**
     * Parse a CSS string into a structured stylesheet.
     * @param css The CSS source code.
     * @return A structured CssStylesheet object.
     */
    static CssStylesheet parse(const std::wstring& css);

    /**
     * Load and parse a CSS file.
     * @param path Path to the CSS file.
     * @return A structured CssStylesheet object.
     */
    static CssStylesheet parse_file(const std::wstring& path);

private:
    /**
     * Internal helper to call Rust FFI.
     * @param css UTF-8 encoded CSS string.
     * @return Parsed JSON-like string from Rust.
     */
    static std::string parse_rust(const std::string& css_utf8);
    
    /**
     * Convert the JSON-like result from Rust to C++ structures.
     * (Simplified parser for the specific JSON format output by Rust)
     */
    static CssStylesheet parse_result(const std::string& json_result);
};

} // namespace hre::css
