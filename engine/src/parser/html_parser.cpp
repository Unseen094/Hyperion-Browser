#include <hre/parser/html_parser.hpp>
#include <windows.h>
#include <stdexcept>
#include <iostream>

namespace hre::parser {

// ---- UTF-8 → wstring conversion ----------------------------------------

std::wstring html_parser::to_wstring(const char* utf8) {
    if (!utf8 || utf8[0] == '\0') return {};

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (size_needed <= 0) return {};

    std::wstring result(size_needed - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], size_needed);
    return result;
}

// ---- Constructor -------------------------------------------------------

html_parser::html_parser(std::string utf8_input)
    : m_input(std::move(utf8_input))
{
    m_document = std::make_unique<dom::document>();
    m_open_elements.push_back(m_document.get());
}

// ---- parse() -----------------------------------------------------------

std::unique_ptr<dom::document> html_parser::parse() {
    HTokenizerHandle* handle = htoken_create(m_input.c_str());
    if (!handle) {
        std::wcerr << L"[HRE] Rust tokenizer returned null handle\n";
        return std::move(m_document);
    }

    HToken tok{};
    while (htoken_next(handle, &tok)) {
        handle_token(tok);
    }

    htoken_free(handle);
    return std::move(m_document);
}

// ---- handle_token() ---------------------------------------------------

void html_parser::handle_token(const HToken& token) {
    switch (token.kind) {

        case HTokenKind::Doctype: {
            break;
        }

        case HTokenKind::StartTag: {
            std::wstring tag_name = to_wstring(token.name);
            auto el = std::make_unique<dom::element>(tag_name);

            for (int i = 0; i < token.attr_count; ++i) {
                std::wstring attr_name  = to_wstring(token.attrs[i].name);
                std::wstring attr_value = to_wstring(token.attrs[i].value);
                el->set_attribute(attr_name, attr_value);
                if (attr_name == L"id") el->set_id(attr_value);
            }

            dom::node* el_ptr = el.get();
            m_open_elements.back()->append_child(std::move(el));

            static constexpr const wchar_t* void_elements[] = {
                L"area", L"base", L"br", L"col", L"embed", L"hr",
                L"img", L"input", L"link", L"meta", L"param",
                L"source", L"track", L"wbr"
            };
            bool is_void = token.self_closing != 0;
            if (!is_void) {
                for (auto* ve : void_elements) {
                    if (tag_name == ve) { is_void = true; break; }
                }
            }
            if (!is_void) {
                m_open_elements.push_back(el_ptr);
            }
            break;
        }

        case HTokenKind::EndTag: {
            if (m_open_elements.size() > 1) {
                std::wstring tag_name = to_wstring(token.name);
                
                // Find the matching tag in the stack (from top to bottom)
                int match_index = -1;
                for (int i = static_cast<int>(m_open_elements.size()) - 1; i > 0; --i) {
                    if (m_open_elements[i]->type() == dom::node_type::element) {
                        auto* el = static_cast<dom::element*>(m_open_elements[i]);
                        if (el->tag_name() == tag_name) {
                            match_index = i;
                            break;
                        }
                    }
                }

                // If found, pop everything up to and including it
                if (match_index != -1) {
                    // Extract script content if we are popping a script
                    if (tag_name == L"script") {
                        auto* top = m_open_elements[match_index];
                        std::wstring script_content;
                        for (const auto& child : top->children()) {
                            if (child->type() == dom::node_type::text) {
                                script_content += static_cast<dom::text_node*>(child.get())->text();
                            }
                        }
                        if (!script_content.empty()) {
                            m_document->add_script(script_content);
                        }
                    }

                    // Unwind stack
                    while (m_open_elements.size() > static_cast<size_t>(match_index)) {
                        m_open_elements.pop_back();
                    }
                }
            }
            break;
        }

        case HTokenKind::Text: {
            std::wstring text = to_wstring(token.data);
            if (!text.empty()) {
                auto text_node = std::make_unique<dom::text_node>(std::move(text));
                m_open_elements.back()->append_child(std::move(text_node));
            }
            break;
        }

        case HTokenKind::Comment:
            break;

        case HTokenKind::Eof:
            break;
    }
}

} // namespace hre::parser
