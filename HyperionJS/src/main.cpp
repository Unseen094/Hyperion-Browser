#include <hjs/lexer/lexer.hpp>
#include <hjs/parser/parser.hpp>
#include <hjs/vm/compiler.hpp>
#include <hjs/vm/vm.hpp>
#include <hjs/debug/profiler.hpp>
#include <hjs/wasm/wasm_module.hpp>
#include <iostream>
#include <string>
#include <fstream>

static void print_usage() {
    std::cout << "Usage: hjs [options] <script>\n"
              << "Options:\n"
              << "  -d, --debug     Enable debug tracing\n"
              << "  -p, --profile   Enable profiling\n"
              << "  -r, --report    Print profile report on exit\n"
              << "  -b, --bench     Benchmark mode (run 5 iterations)\n"
              << "  -f <file>       Execute script from file\n"
              << "  -w <file>       Load WASM module\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    bool debug_mode = false;
    bool profile_mode = false;
    bool report_mode = false;
    bool bench_mode = false;
    std::string script;
    std::string wasm_file;
    bool from_file = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--debug") debug_mode = true;
        else if (arg == "-p" || arg == "--profile") profile_mode = true;
        else if (arg == "-r" || arg == "--report") report_mode = true;
        else if (arg == "-b" || arg == "--bench") bench_mode = true;
        else if (arg == "-f" && i + 1 < argc) { from_file = true; script = argv[++i]; }
        else if (arg == "-w" && i + 1 < argc) { wasm_file = argv[++i]; }
        else script = arg;
    }

    if (script.empty()) {
        std::cerr << "Error: No script provided\n";
        return 1;
    }

    if (!wasm_file.empty()) {
        std::ifstream wf(wasm_file, std::ios::binary);
        if (!wf) { std::cerr << "Error: Cannot open WASM file\n"; return 1; }
        std::vector<uint8_t> wasm_data((std::istreambuf_iterator<char>(wf)), {});
        auto mod = std::make_shared<hjs::wasm::WasmModule>();
        if (mod->parse(wasm_data)) {
            std::cout << "WASM module loaded: " << mod->exports().size() << " exports\n";
        } else {
            std::cerr << "WASM parse error: " << mod->error() << "\n";
            return 1;
        }
    }

    hjs::vm::VM::set_debug_mode(debug_mode);
    hjs::vm::VM::set_profile_mode(profile_mode);

    if (profile_mode) {
        hjs::debug::Profiler::instance().enable();
    }

    std::string source_utf8;
    if (from_file) {
        std::ifstream f(script);
        if (!f) { std::cerr << "Error: Cannot open file\n"; return 1; }
        source_utf8 = std::string((std::istreambuf_iterator<char>(f)), {});
    } else {
        source_utf8 = script;
    }

    auto run = [&]() -> bool {
        std::wstring source(source_utf8.begin(), source_utf8.end());

        hjs::lexer::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        hjs::parser::Parser parser(tokens);
        auto statements = parser.parse();

        if (statements.empty()) return true;

        hjs::vm::Chunk chunk;
        hjs::vm::Compiler compiler(chunk);

        for (auto& stmt : statements) {
            if (stmt) compiler.compile(*stmt);
        }

        chunk.write((uint8_t)hjs::vm::OpCode::Return);

        hjs::vm::VM vm;
        auto result = vm.interpret(chunk);
        vm.drain_microtask_queue();
        return result == hjs::vm::VM::InterpretResult::Ok;
    };

    if (bench_mode) {
        int iterations = 5;
        for (int i = 0; i < iterations; i++) {
            run();
        }
        std::cout << "\nBenchmark complete: " << iterations << " iterations\n";
    } else {
        run();
    }

    if (report_mode && profile_mode) {
        std::cout << hjs::debug::Profiler::instance().report();
    }

    return 0;
}
