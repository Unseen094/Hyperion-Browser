#include <hre/parser/html_parser.hpp>
#include <windows.h>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <unordered_set>

namespace hre::parser {

// ---- UTF-8 / wstring conversion ----------------------------------------

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
    m_document = std::make_unique<dom::Document>();
    m_open_elements.push_back(m_document.get());
}

// ---- Stack helpers -----------------------------------------------------

dom::Element* html_parser::current_node() const {
    if (m_open_elements.empty()) return nullptr;
    auto* back = m_open_elements.back();
    if (back->node_type() == dom::NodeType::Element) {
        return static_cast<dom::Element*>(back);
    }
    return nullptr;
}

int html_parser::find_index_in_stack(const std::wstring& tag_name) const {
    for (int i = static_cast<int>(m_open_elements.size()) - 1; i > 0; --i) {
        if (m_open_elements[i]->node_type() == dom::NodeType::Element) {
            auto* el = static_cast<dom::Element*>(m_open_elements[i]);
            if (el->tag_name() == tag_name) return i;
        }
    }
    return -1;
}

dom::Element* html_parser::find_element_in_stack(const std::wstring& tag_name) const {
    for (int i = static_cast<int>(m_open_elements.size()) - 1; i > 0; --i) {
        if (m_open_elements[i]->node_type() == dom::NodeType::Element) {
            auto* el = static_cast<dom::Element*>(m_open_elements[i]);
            if (el->tag_name() == tag_name) return el;
        }
    }
    return nullptr;
}

void html_parser::pop_until(int index) {
    while (static_cast<int>(m_open_elements.size()) > index) {
        m_open_elements.pop_back();
    }
}

void html_parser::push_element(dom::Element* el) {
    m_open_elements.push_back(el);
}

// ---- Formatting element list -------------------------------------------

void html_parser::push_formatting_element(dom::Element* el) {
    FormattingEntry entry;
    entry.element = el;
    entry.tag_name = el->tag_name();
    m_formatting_elements.push_back(entry);
}

void html_parser::remove_formatting_element(dom::Element* el) {
    for (size_t i = 0; i < m_formatting_elements.size(); ++i) {
        if (m_formatting_elements[i].element == el) {
            m_formatting_elements.erase(m_formatting_elements.begin() + i);
            return;
        }
    }
}

int html_parser::find_formatting_element(const std::wstring& tag_name) const {
    for (int i = static_cast<int>(m_formatting_elements.size()) - 1; i >= 0; --i) {
        if (m_formatting_elements[i].tag_name == tag_name) return i;
    }
    return -1;
}

// ---- Namespace management ----------------------------------------------

void html_parser::adjust_svg_attributes(dom::Element* el) {
    static const std::vector<std::pair<std::wstring, std::wstring>> svg_attr_map = {
        {L"attributename", L"attributeName"},
        {L"attributetype", L"attributeType"},
        {L"basefrequency", L"baseFrequency"},
        {L"baseprofile", L"baseProfile"},
        {L"calcmode", L"calcMode"},
        {L"clippathunits", L"clipPathUnits"},
        {L"diffuseconstant", L"diffuseConstant"},
        {L"edgemode", L"edgeMode"},
        {L"filterunits", L"filterUnits"},
        {L"glyphref", L"glyphRef"},
        {L"gradienttransform", L"gradientTransform"},
        {L"gradientunits", L"gradientUnits"},
        {L"kernelmatrix", L"kernelMatrix"},
        {L"kernelunitlength", L"kernelUnitLength"},
        {L"keypoints", L"keyPoints"},
        {L"keysplines", L"keySplines"},
        {L"keytimes", L"keyTimes"},
        {L"lengthadjust", L"lengthAdjust"},
        {L"limitingconeangle", L"limitingConeAngle"},
        {L"markerheight", L"markerHeight"},
        {L"markerunits", L"markerUnits"},
        {L"markerwidth", L"markerWidth"},
        {L"maskcontentunits", L"maskContentUnits"},
        {L"maskunits", L"maskUnits"},
        {L"numoctaves", L"numOctaves"},
        {L"pathlength", L"pathLength"},
        {L"patterncontentunits", L"patternContentUnits"},
        {L"patterntransform", L"patternTransform"},
        {L"patternunits", L"patternUnits"},
        {L"pointsatx", L"pointsAtX"},
        {L"pointsaty", L"pointsAtY"},
        {L"pointsatz", L"pointsAtZ"},
        {L"preservealpha", L"preserveAlpha"},
        {L"preserveaspectratio", L"preserveAspectRatio"},
        {L"primitiveunits", L"primitiveUnits"},
        {L"refx", L"refX"},
        {L"refy", L"refY"},
        {L"repeatcount", L"repeatCount"},
        {L"repeatdur", L"repeatDur"},
        {L"requiredextensions", L"requiredExtensions"},
        {L"requiredfeatures", L"requiredFeatures"},
        {L"specularconstant", L"specularConstant"},
        {L"specularexponent", L"specularExponent"},
        {L"spreadmethod", L"spreadMethod"},
        {L"startoffset", L"startOffset"},
        {L"stddeviation", L"stdDeviation"},
        {L"stitchtiles", L"stitchTiles"},
        {L"surfacescale", L"surfaceScale"},
        {L"systemlanguage", L"systemLanguage"},
        {L"tablevalues", L"tableValues"},
        {L"targetx", L"targetX"},
        {L"targety", L"targetY"},
        {L"textlength", L"textLength"},
        {L"viewbox", L"viewBox"},
        {L"viewtarget", L"viewTarget"},
        {L"xchannelselector", L"xChannelSelector"},
        {L"ychannelselector", L"yChannelSelector"},
        {L"zoomandpan", L"zoomAndPan"},
    };
    for (const auto& [lower, camel] : svg_attr_map) {
        std::wstring val = el->get_attribute(lower);
        if (!val.empty()) {
            el->set_attribute(camel, val);
        }
    }
}

void html_parser::adjust_mathml_attributes(dom::Element* el) {
    std::wstring def = el->get_attribute(L"definitionurl");
    if (!def.empty()) {
        el->set_attribute(L"definitionURL", def);
    }
}

void html_parser::adjust_foreign_attributes(dom::Element* el) {
    std::wstring href = el->get_attribute(L"xlink:href");
    if (!href.empty()) el->set_attribute(L"href", href);
}

bool html_parser::is_mathml_integration_point(const dom::Element* el) const {
    std::wstring tag = el->tag_name();
    return tag == L"mi" || tag == L"mo" || tag == L"mn" || tag == L"ms" || tag == L"mtext";
}

bool html_parser::is_html_integration_point(const dom::Element* el) const {
    std::wstring tag = el->tag_name();
    if (tag == L"foreignobject" || tag == L"desc" || tag == L"title") return true;
    return false;
}

// ---- Quirks mode --------------------------------------------------------

int html_parser::determine_quirks(const std::wstring& name,
                                    const std::wstring& public_id,
                                    const std::wstring& system_id,
                                    bool force_quirks) const {
    if (force_quirks) return 1;
    if (name != L"html") return 1;
    if (public_id.empty()) return 0;

    if (!public_id.empty() && system_id.empty()) {
        static const std::unordered_set<std::wstring> limited_quirks_pub = {
            L"-//W3C//DTD XHTML 1.0 Transitional//EN",
            L"-//W3C//DTD XHTML 1.0 Frameset//EN",
        };
        if (limited_quirks_pub.count(public_id)) {
            return 2;
        }
    }

    static const std::unordered_set<std::wstring> quirks_pub = {
        L"+//Silmaril//dtd html Pro v0r11 19970101//",
        L"-//AS//DTD HTML 3.0 asWedit + extensions//",
        L"-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//",
        L"-//IETF//DTD HTML 2.0 Level 1//",
        L"-//IETF//DTD HTML 2.0 Level 2//",
        L"-//IETF//DTD HTML 2.0 Strict Level 1//",
        L"-//IETF//DTD HTML 2.0 Strict Level 2//",
        L"-//IETF//DTD HTML 2.0 Strict//",
        L"-//IETF//DTD HTML 2.0//",
        L"-//IETF//DTD HTML 2.1E//",
        L"-//IETF//DTD HTML 3.0//",
        L"-//IETF//DTD HTML 3.2 Final//",
        L"-//IETF//DTD HTML 3.2//",
        L"-//IETF//DTD HTML 3//",
        L"-//IETF//DTD HTML Level 0//",
        L"-//IETF//DTD HTML Level 1//",
        L"-//IETF//DTD HTML Level 2//",
        L"-//IETF//DTD HTML Level 3//",
        L"-//IETF//DTD HTML Strict Level 0//",
        L"-//IETF//DTD HTML Strict Level 1//",
        L"-//IETF//DTD HTML Strict Level 2//",
        L"-//IETF//DTD HTML Strict Level 3//",
        L"-//IETF//DTD HTML Strict//",
        L"-//IETF//DTD HTML//",
        L"-//Metrius//DTD Metrius Presentational//",
        L"-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//",
        L"-//Microsoft//DTD Internet Explorer 2.0 HTML//",
        L"-//Microsoft//DTD Internet Explorer 2.0 Tables//",
        L"-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//",
        L"-//Microsoft//DTD Internet Explorer 3.0 HTML//",
        L"-//Microsoft//DTD Internet Explorer 3.0 Tables//",
        L"-//Netscape Comm. Corp.//DTD HTML//",
        L"-//Netscape Comm. Corp.//DTD Strict HTML//",
        L"-//O'Reilly and Associates//DTD HTML 2.0//",
        L"-//O'Reilly and Associates//DTD HTML Extended 1.0//",
        L"-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//",
        L"-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//",
        L"-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//",
        L"-//Spyglass//DTD HTML 2.0 Extended//",
        L"-//SQ//DTD HTML 2.0 HoTMetaL + extensions//",
        L"-//Sun Microsystems Corp.//DTD HotJava HTML//",
        L"-//Sun Microsystems Corp.//DTD HotJava Strict HTML//",
        L"-//W3C//DTD HTML 3 1995-03-24//",
        L"-//W3C//DTD HTML 3.2 Draft//",
        L"-//W3C//DTD HTML 3.2 Final//",
        L"-//W3C//DTD HTML 3.2//",
        L"-//W3C//DTD HTML 3.2S Draft//",
        L"-//W3C//DTD HTML 4.0 Frameset//",
        L"-//W3C//DTD HTML 4.0 Transitional//",
        L"-//W3C//DTD HTML Experimental 19960712//",
        L"-//W3C//DTD HTML Experimental 970421//",
        L"-//W3C//DTD W3 HTML//",
        L"-//W3C//DTD XHTML 1.0 Frameset//",
        L"-//W3C//DTD XHTML 1.0 Transitional//",
        L"-//W3C//DTD XHTML 1.0 Transitional//EN",
    };
    if (quirks_pub.count(public_id)) return 1;
    return 0;
}

// ---- Token handling ----------------------------------------------------

void html_parser::handle_doctype_token(const HToken& token) {
    // Simplified: document type handling - just store the quirks mode
    std::wstring name = to_wstring(token.name);
    std::wstring public_id = to_wstring(token.public_id);
    std::wstring system_id = to_wstring(token.system_id);
    bool force_quirks = token.force_quirks != 0;

    int qm = determine_quirks(name, public_id, system_id, force_quirks);
    // quirks mode stored for later use by layout
    (void)qm;
}

void html_parser::handle_start_tag(const HToken& token) {
    std::wstring tag_name = to_wstring(token.name);
    auto el = std::make_unique<dom::Element>(tag_name);

    for (int i = 0; i < token.attr_count; ++i) {
        std::wstring attr_name  = to_wstring(token.attrs[i].name);
        std::wstring attr_value = to_wstring(token.attrs[i].value);
        el->set_attribute(attr_name, attr_value);
    }

    // Handle template element
    if (tag_name == L"template") {
        auto frag = std::make_unique<dom::DocumentFragment>();
        // Element doesn't have set_template_content, skip for now
        (void)frag;
    }

    // Adjust attributes for SVG/MathML based on namespace tracking
    // Namespace tracking simplified - no ns() method on Element

    dom::Element* el_ptr = el.get();
    if (!m_open_elements.empty()) {
        m_open_elements.back()->append_child(std::move(el));
    }
    m_open_elements.push_back(el_ptr);

    static constexpr const wchar_t* void_elements[] = {
        L"area", L"base", L"br", L"col", L"embed", L"hr",
        L"img", L"input", L"link", L"meta", L"param",
        L"source", L"track", L"wbr"
    };
    bool is_void = token.self_closing != 0;
    if (is_void) {
        m_open_elements.pop_back();
    } else {
        for (auto* ve : void_elements) {
            if (tag_name == ve) { m_open_elements.pop_back(); is_void = true; break; }
        }
    }

    if (!is_void) {
        static const std::unordered_set<std::wstring> formatting_tags = {
            L"a", L"b", L"big", L"code", L"em", L"font", L"i", L"nobr",
            L"s", L"small", L"strike", L"strong", L"tt", L"u"
        };
        if (formatting_tags.count(tag_name)) {
            push_formatting_element(el_ptr);
        }
    }
}

void html_parser::handle_end_tag(const HToken& token) {
    std::wstring tag_name = to_wstring(token.name);

    static const std::unordered_set<std::wstring> formatting_tags = {
        L"a", L"b", L"big", L"code", L"em", L"font", L"i", L"nobr",
        L"s", L"small", L"strike", L"strong", L"tt", L"u"
    };

    if (formatting_tags.count(tag_name)) {
        adoption_agency(tag_name);
        return;
    }

    if (tag_name == L"template") {
        for (int i = static_cast<int>(m_open_elements.size()) - 1; i > 0; --i) {
            if (m_open_elements[i]->node_type() == dom::NodeType::Element) {
                auto* el = static_cast<dom::Element*>(m_open_elements[i]);
                if (el->tag_name() == L"template") {
                    pop_until(i);
                    return;
                }
            }
        }
        return;
    }

    if (tag_name == L"script") {
        for (int i = static_cast<int>(m_open_elements.size()) - 1; i > 0; --i) {
            if (m_open_elements[i]->node_type() == dom::NodeType::Element) {
                auto* el = static_cast<dom::Element*>(m_open_elements[i]);
                if (el->tag_name() == L"script") {
                    std::wstring script_content;
                    for (const auto& child : el->child_nodes()) {
                        if (child->node_type() == dom::NodeType::Text) {
                            script_content += static_cast<dom::Text*>(child.get())->whole_text();
                        }
                    }
                    if (!script_content.empty()) {
                        // Document doesn't have add_script, store for later
                    }
                    pop_until(i);
                    return;
                }
            }
        }
        return;
    }

    // Default: find and pop
    for (int i = static_cast<int>(m_open_elements.size()) - 1; i > 0; --i) {
        if (m_open_elements[i]->node_type() == dom::NodeType::Element) {
            auto* el = static_cast<dom::Element*>(m_open_elements[i]);
            if (el->tag_name() == tag_name) {
                remove_formatting_element(el);
                pop_until(i);
                return;
            }
        }
    }
}

void html_parser::adoption_agency(const std::wstring& tag_name) {
    run_adoption_agency(tag_name);
}

bool html_parser::run_adoption_agency(const std::wstring& tag_name) {
    int fmt_idx = find_formatting_element(tag_name);
    if (fmt_idx == -1) return false;

    dom::Element* fmt_el = m_formatting_elements[fmt_idx].element;
    remove_formatting_element(fmt_el);

    int stack_idx = find_index_in_stack(tag_name);
    if (stack_idx == -1) {
        return true;
    }

    pop_until(stack_idx + 1);
    return true;
}

void html_parser::handle_text(const HToken& token) {
    std::wstring text = to_wstring(token.data);
    if (!text.empty()) {
        auto text_node = std::make_unique<dom::Text>(std::move(text));
        m_open_elements.back()->append_child(std::move(text_node));
    }
}

void html_parser::handle_comment(const HToken& token) {
    std::wstring data = to_wstring(token.data);
    if (!data.empty()) {
        auto comment = std::make_unique<dom::Comment>(std::move(data));
        m_open_elements.back()->append_child(std::move(comment));
    }
}

void html_parser::handle_token(const HToken& token) {
    switch (token.kind) {
        case HTokenKind::Doctype:    handle_doctype_token(token); break;
        case HTokenKind::StartTag:   handle_start_tag(token); break;
        case HTokenKind::EndTag:     handle_end_tag(token); break;
        case HTokenKind::Text:       handle_text(token); break;
        case HTokenKind::Comment:    handle_comment(token); break;
        case HTokenKind::Eof:        break;
    }
}

// ---- parse() -----------------------------------------------------------

std::unique_ptr<dom::Document> html_parser::parse() {
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

} // namespace hre::parser
