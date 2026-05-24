#include "hre/css/css_parser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <codecvt>
#include <locale>

namespace hre::css {

// Helper to convert wstring to utf8
static std::string wstring_to_utf8(const std::wstring& ws) {
    std::string str(ws.length(), 0);
    size_t len = 0;
    for (wchar_t wc : ws) {
        if (wc < 0x80) {
            str[len++] = (char)wc;
        } else if (wc < 0x800) {
            str[len++] = (0xC0 | (wc >> 6));
            str[len++] = (0x80 | (wc & 0x3F));
        } else {
            str[len++] = (0xE0 | (wc >> 12));
            str[len++] = (0x80 | ((wc >> 6) & 0x3F));
            str[len++] = (0x80 | (wc & 0x3F));
        }
    }
    return str.substr(0, len);
}

// Helper to convert utf8 to wstring
static std::wstring utf8_to_wstring(const std::string& s) {
    std::wstring ws;
    for (size_t i = 0; i < s.length(); ) {
        wchar_t wc;
        unsigned char c = s[i];
        if (c < 0x80) {
            wc = c;
            i++;
        } else if (c < 0xE0) {
            wc = ((c & 0x1F) << 6) | (s[i+1] & 0x3F);
            i += 2;
        } else {
            wc = ((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F);
            i += 3;
        }
        ws += wc;
    }
    return ws;
}

std::string Parser::parse_rust(const std::string& css_utf8) {
    const char* result_cstr = rust_parse_css(css_utf8.c_str());
    if (result_cstr == nullptr) {
        return "{}";
    }
    std::string result(result_cstr);
    rust_free_css_string(const_cast<char*>(result_cstr));
    return result;
}

// Simple JSON-like parser for Rust output format: { "rules": [ { "selectors": [...], "declarations": [...] }, ... ] }
CssStylesheet Parser::parse_result(const std::string& json_result) {
    CssStylesheet stylesheet;
    
    // Very basic parsing for the specific format output by Rust
    // Format: { "rules": [ { "selectors": ["div"], "declarations": ["color: red"] } ] }
    
    size_t rules_pos = json_result.find("\"rules\"");
    if (rules_pos == std::string::npos) {
        return stylesheet;
    }
    
    size_t array_start = json_result.find('[', rules_pos);
    if (array_start == std::string::npos) {
        return stylesheet;
    }
    
    size_t pos = array_start + 1;
    int brace_count = 0;
    bool in_string = false;
    
    while (pos < json_result.length()) {
        char c = json_result[pos];
        
        if (c == '"' && (pos == 0 || json_result[pos-1] != '\\')) {
            in_string = !in_string;
        }
        
        if (!in_string) {
            if (c == '{') {
                if (brace_count == 0) {
                    // Start of a rule
                    CssRule rule;
                    
                    // Find selectors
                    size_t sel_start = json_result.find("\"selectors\"", pos);
                    if (sel_start != std::string::npos) {
                        size_t sel_array_start = json_result.find('[', sel_start);
                        if (sel_array_start != std::string::npos) {
                            size_t sel_array_end = json_result.find(']', sel_array_start);
                            std::string selectors_str = json_result.substr(sel_array_start + 1, sel_array_end - sel_array_start - 1);
                            
                            // Parse individual selectors
                            size_t sel_pos = 0;
                            while (sel_pos < selectors_str.length()) {
                                size_t quote_start = selectors_str.find('"', sel_pos);
                                if (quote_start == std::string::npos) break;
                                size_t quote_end = selectors_str.find('"', quote_start + 1);
                                if (quote_end == std::string::npos) break;
                                
                                std::wstring selector = utf8_to_wstring(selectors_str.substr(quote_start + 1, quote_end - quote_start - 1));
                                rule.selectors.push_back(selector);
                                sel_pos = quote_end + 1;
                            }
                        }
                    }
                    
                    // Find declarations
                    size_t decl_start = json_result.find("\"declarations\"", pos);
                    if (decl_start != std::string::npos) {
                        size_t decl_array_start = json_result.find('[', decl_start);
                        if (decl_array_start != std::string::npos) {
                            size_t decl_array_end = json_result.find(']', decl_array_start);
                            std::string declarations_str = json_result.substr(decl_array_start + 1, decl_array_end - decl_array_start - 1);
                            
                            // Parse individual declarations
                            size_t decl_pos = 0;
                            while (decl_pos < declarations_str.length()) {
                                size_t quote_start = declarations_str.find('"', decl_pos);
                                if (quote_start == std::string::npos) break;
                                size_t quote_end = declarations_str.find('"', quote_start + 1);
                                if (quote_end == std::string::npos) break;
                                
                                std::string decl_str = declarations_str.substr(quote_start + 1, quote_end - quote_start - 1);
                                size_t colon_pos = decl_str.find(':');
                                if (colon_pos != std::string::npos) {
                                    CssDeclaration decl;
                                    decl.property = decl_str.substr(0, colon_pos);
                                    std::string value_part = decl_str.substr(colon_pos + 1);
                                    
                                    // Trim whitespace
                                    size_t start = value_part.find_first_not_of(" ");
                                    if (start != std::string::npos) {
                                        decl.value = value_part.substr(start);
                                    } else {
                                        decl.value = "";
                                    }
                                    decl.important = false; // TODO: Parse !important
                                    rule.declarations.push_back(decl);
                                }
                                
                                decl_pos = quote_end + 1;
                            }
                        }
                    }
                    
                    stylesheet.rules.push_back(rule);
                }
                brace_count++;
            } else if (c == '}') {
                brace_count--;
                if (brace_count == 0 && !stylesheet.rules.empty()) {
                    // End of current rule
                    pos++;
                    continue;
                }
            }
        }
        pos++;
    }
    
    return stylesheet;
}

CssStylesheet Parser::parse(const std::wstring& css) {
    std::string css_utf8 = wstring_to_utf8(css);
    std::string result = parse_rust(css_utf8);
    return parse_result(result);
}

CssStylesheet Parser::parse_file(const std::wstring& path) {
    std::wifstream file(path.c_str());
    if (!file.is_open()) {
        return CssStylesheet();
    }
    
    std::wstringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}

} // namespace hre::css
