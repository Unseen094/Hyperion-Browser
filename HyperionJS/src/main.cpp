#include <hjs/lexer/lexer.hpp>
#include <hjs/parser/parser.hpp>
#include <hjs/vm/compiler.hpp>
#include <hjs/vm/vm.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: hjs <script>" << std::endl;
        return 1;
    }

    std::string source_utf8 = argv[1];
    std::wstring source(source_utf8.begin(), source_utf8.end());

    hjs::lexer::Lexer lexer(source);
    auto tokens = lexer.tokenize();

    hjs::parser::Parser parser(tokens);
    auto statements = parser.parse();

    if (statements.empty()) {
        return 0;
    }

    hjs::vm::Chunk chunk;
    hjs::vm::Compiler compiler(chunk);
    
    for (auto& stmt : statements) {
        if (stmt) compiler.compile(*stmt);
    }
    
    chunk.write((uint8_t)hjs::vm::OpCode::Return);

    hjs::vm::VM vm;
    vm.interpret(chunk);

    return 0;
}
