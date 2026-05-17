#pragma once

#include <hre/dom/node.hpp>
#include <hre/parser/html_tokenizer_bridge.hpp>
#include <memory>
#include <stack>
#include <string>

namespace hre::parser {

/// HTML parser that drives the Rust tokenizer via the C FFI bridge.
///
/// Usage:
///   html_parser p(utf8_source);
///   auto doc = p.parse();
///
/// The input must be UTF-8. The C++ DOM is populated from the Rust token stream.
class html_parser {
public:
    explicit html_parser(std::string utf8_input);

    std::unique_ptr<dom::document> parse();

private:
    void handle_token(const HToken& token);

    /// Convert a UTF-8 C string (null-terminated) to wstring for the DOM layer.
    static std::wstring to_wstring(const char* utf8);

    std::string m_input;   // kept alive for the lifetime of the parse
    std::unique_ptr<dom::document> m_document;
    std::vector<dom::node*> m_open_elements;
};

} // namespace hre::parser
