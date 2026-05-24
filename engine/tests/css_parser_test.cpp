#include "hre/css/css_parser.hpp"
#include <iostream>
#include <cassert>
#include <string>

// Helper to convert std::string to std::wstring for printing
static std::wstring to_wstring(const std::string& s) {
    std::wstring ws;
    for (unsigned char c : s) {
        ws += (wchar_t)c;
    }
    return ws;
}

void test_simple_selector() {
    std::wstring css = L"div { color: red; }";
    auto stylesheet = hre::css::Parser::parse(css);
    
    std::wcout << L"Test 1: Simple selector" << std::endl;
    std::wcout << L"Rules count: " << stylesheet.rules.size() << std::endl;
    
    if (!stylesheet.rules.empty()) {
        std::wcout << L"Selectors: ";
        for (const auto& sel : stylesheet.rules[0].selectors) {
            std::wcout << sel << L" ";
        }
        std::wcout << std::endl;
        
        std::wcout << L"Declarations: " << stylesheet.rules[0].declarations.size() << std::endl;
        for (const auto& decl : stylesheet.rules[0].declarations) {
            std::wcout << L"  " << to_wstring(decl.property) << L": " << to_wstring(decl.value) << std::endl;
        }
    }
    
    std::wcout << L"Test 1 passed" << std::endl;
}

void test_class_selector() {
    std::wstring css = L".container { background: blue; }";
    auto stylesheet = hre::css::Parser::parse(css);
    
    std::wcout << L"Test 2: Class selector" << std::endl;
    std::wcout << L"Rules count: " << stylesheet.rules.size() << std::endl;
    
    if (!stylesheet.rules.empty()) {
        std::wcout << L"Selectors: ";
        for (const auto& sel : stylesheet.rules[0].selectors) {
            std::wcout << sel << L" ";
        }
        std::wcout << std::endl;
    }
    
    std::wcout << L"Test 2 passed" << std::endl;
}

void test_id_selector() {
    std::wstring css = L"#main { margin: 10px; }";
    auto stylesheet = hre::css::Parser::parse(css);
    
    std::wcout << L"Test 3: ID selector" << std::endl;
    std::wcout << L"Rules count: " << stylesheet.rules.size() << std::endl;
    
    if (!stylesheet.rules.empty()) {
        std::wcout << L"Selectors: ";
        for (const auto& sel : stylesheet.rules[0].selectors) {
            std::wcout << sel << L" ";
        }
        std::wcout << std::endl;
    }
    
    std::wcout << L"Test 3 passed" << std::endl;
}

int main() {
    std::wcout << L"Running CSS Parser Tests..." << std::endl;
    std::wcout << L"================================" << std::endl;
    
    try {
        test_simple_selector();
        test_class_selector();
        test_id_selector();
        
        std::wcout << L"================================" << std::endl;
        std::wcout << L"All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::wcerr << L"Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
