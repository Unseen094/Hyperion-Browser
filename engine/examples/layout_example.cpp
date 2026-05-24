// Module 1: Foundation & Text - Example Usage
// This example demonstrates Phases 15-17 implementation

#include "hre/layout/layout_engine.hpp"
#include "hre/text/font_engine.hpp"
#include "hre/dom/node.hpp"
#include <iostream>
#include <memory>

using namespace hre::layout;
using namespace hre::dom;
using namespace hre::text;

void demonstrate_block_layout() {
    std::wcout << L"=== Block Layout Example ===" << std::endl;
    
    // Create a container div
    auto container = std::make_shared<LayoutNode>(nullptr);
    container->style.display = L"block";
    container->style.width = css::CssValue{400, css::CssUnit::Pixel, L"400px"};
    container->style.height = css::CssValue{300, css::CssUnit::Pixel, L"300px"};
    container->style.padding_top = css::CssValue{20, css::CssUnit::Pixel, L"20px"};
    container->style.padding_left = css::CssValue{20, css::CssUnit::Pixel, L"20px"};
    
    // Create child blocks
    auto child1 = std::make_shared<LayoutNode>(nullptr);
    child1->style.display = L"block";
    child1->style.width = css::CssValue{200, css::CssUnit::Pixel, L"200px"};
    child1->style.height = css::CssValue{100, css::CssUnit::Pixel, L"100px"};
    child1->style.margin_top = css::CssValue{10, css::CssUnit::Pixel, L"10px"};
    
    auto child2 = std::make_shared<LayoutNode>(nullptr);
    child2->style.display = L"block";
    child2->style.width = css::CssValue{150, css::CssUnit::Pixel, L"150px"};
    child2->style.height = css::CssValue{80, css::CssUnit::Pixel, L"80px"};
    
    container->children.push_back(child1);
    container->children.push_back(child2);
    
    // Perform layout
    LayoutEngine engine;
    engine.layout_tree(container, 400, 300);
    
    // Output results
    std::wcout << L"Container: " << container->content_box.width << L"x" 
               << container->content_box.height << std::endl;
    std::wcout << L"Child1 position: (" << child1->content_box.x << L", " 
               << child1->content_box.y << L")" << std::endl;
    std::wcout << L"Child2 position: (" << child2->content_box.x << L", " 
               << child2->content_box.y << L")" << std::endl;
}

void demonstrate_inline_layout() {
    std::wcout << L"\n=== Inline Layout Example ===" << std::endl;
    
    // Create inline container
    auto container = std::make_shared<LayoutNode>(nullptr);
    container->style.display = L"inline-block";
    container->style.width = css::CssValue{300, css::CssUnit::Pixel, L"300px"};
    
    // Create inline children (simulating text spans)
    for (int i = 0; i < 5; i++) {
        auto span = std::make_shared<LayoutNode>(nullptr);
        span->style.display = L"inline-block";
        span->style.width = css::CssValue{50, css::CssUnit::Pixel, L"50px"};
        span->style.height = css::CssValue{30, css::CssUnit::Pixel, L"30px"};
        container->children.push_back(span);
    }
    
    LayoutEngine engine;
    engine.layout_tree(container, 300, 100);
    
    std::wcout << L"Inline children laid out horizontally" << std::endl;
}

void demonstrate_text_shaping() {
    std::wcout << L"\n=== Text Shaping Example ===" << std::endl;
    
    // Get font from engine
    auto& font_engine = FontEngine::instance();
    auto font = font_engine.get_font(L"Segoe UI", 16.0f);
    
    if (font && font->is_valid()) {
        std::wcout << L"Font loaded successfully" << std::endl;
        
        // Shape some text
        std::wstring text = L"Hello, World!";
        auto glyphs = font->shape_text(text);
        
        std::wcout << L"Text: " << text << std::endl;
        std::wcout << L"Glyph count: " << glyphs.size() << std::endl;
        std::wcout << L"Total width: " << font->get_text_width(text) << L"px" << std::endl;
        
        // Show individual glyph metrics
        for (size_t i = 0; i < glyphs.size() && i < 3; i++) {
            std::wcout << L"  Glyph[" << i << L"]: codepoint=" << glyphs[i].codepoint
                      << L", x=" << glyphs[i].x_offset
                      << L", width=" << glyphs[i].width << std::endl;
        }
    } else {
        std::wcout << L"Font loading failed (expected on non-Windows or if font not available)" << std::endl;
    }
}

void demonstrate_flexbox() {
    std::wcout << L"\n=== Flexbox Layout Example ===" << std::endl;
    
    auto container = std::make_shared<LayoutNode>(nullptr);
    container->style.display = L"flex";
    container->style.flex_direction = L"row";
    container->style.width = css::CssValue{400, css::CssUnit::Pixel, L"400px"};
    container->style.height = css::CssValue{200, css::CssUnit::Pixel, L"200px"};
    
    // Create flex items
    for (int i = 0; i < 3; i++) {
        auto item = std::make_shared<LayoutNode>(nullptr);
        item->style.width = css::CssValue{100, css::CssUnit::Pixel, L"100px"};
        item->style.height = css::CssValue{50, css::CssUnit::Pixel, L"50px"};
        container->children.push_back(item);
    }
    
    LayoutEngine engine;
    engine.layout_tree(container, 400, 200);
    
    std::wcout << L"Flex container: " << container->content_box.width << L"x" 
               << container->content_box.height << std::endl;
    std::wcout << L"Items distributed across container" << std::endl;
}

int main() {
    std::wcout << L"Hyperion Layout Engine - Module 1 Demonstration" << std::endl;
    std::wcout << L"Phases 15-17: Text Shaping, Inline Formatting, Block Layout" << std::endl;
    std::wcout << L"=========================================================" << std::endl;
    
    demonstrate_block_layout();
    demonstrate_inline_layout();
    demonstrate_text_shaping();
    demonstrate_flexbox();
    
    std::wcout << L"\n=== All demonstrations completed ===" << std::endl;
    return 0;
}
