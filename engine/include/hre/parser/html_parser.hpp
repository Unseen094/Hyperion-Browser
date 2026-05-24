#pragma once

#include <hre/dom/node.hpp>
#include <hre/parser/html_tokenizer_bridge.hpp>
#include <memory>
#include <stack>
#include <string>
#include <vector>

namespace hre::parser {

class html_parser {
public:
    explicit html_parser(std::string utf8_input);

    std::unique_ptr<dom::Document> parse();

private:
    // Token handling
    void handle_token(const HToken& token);
    void handle_start_tag(const HToken& token);
    void handle_end_tag(const HToken& token);
    void handle_text(const HToken& token);
    void handle_comment(const HToken& token);
    void handle_doctype_token(const HToken& token);

    // Stack helpers
    dom::Element* current_node() const;
    dom::Element* find_element_in_stack(const std::wstring& tag_name) const;
    int find_index_in_stack(const std::wstring& tag_name) const;
    void pop_until(int index);
    void push_element(dom::Element* el);

    // Adoption Agency Algorithm
    void adoption_agency(const std::wstring& tag_name);
    bool run_adoption_agency(const std::wstring& tag_name);

    // Formatting element list
    struct FormattingEntry {
        dom::Element* element;
        std::wstring tag_name;
    };
    void push_formatting_element(dom::Element* el);
    void remove_formatting_element(dom::Element* el);
    int find_formatting_element(const std::wstring& tag_name) const;

    // Namespace management
    void adjust_svg_attributes(dom::Element* el);
    void adjust_mathml_attributes(dom::Element* el);
    void adjust_foreign_attributes(dom::Element* el);
    bool is_mathml_integration_point(const dom::Element* el) const;
    bool is_html_integration_point(const dom::Element* el) const;

    // Raw text tracking
    void process_raw_text(const std::wstring& tag_name);

    // Quirks mode determination
    int determine_quirks(const std::wstring& name,
                          const std::wstring& public_id,
                          const std::wstring& system_id,
                          bool force_quirks) const;

    static std::wstring to_wstring(const char* utf8);

    std::string m_input;
    std::unique_ptr<dom::Document> m_document;
    std::vector<dom::Node*> m_open_elements;
    std::vector<FormattingEntry> m_formatting_elements;
    bool m_scripting_enabled = true;
};

} // namespace hre::parser
