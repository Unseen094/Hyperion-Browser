#pragma once

#include <hjs/vm/bytecode.hpp>
#include <hjs/core/value.hpp>
#include <hjs/runtime/environment.hpp>
#include <vector>
#include <cstdint>

namespace hjs { class Upvalue; }

namespace hjs::vm {

class VM {
public:
    enum class InterpretResult { Ok, CompileError, RuntimeError };

    InterpretResult interpret(const hjs::vm::Chunk& chunk);

    static hjs::runtime::Environment m_globals;

    static void setup_console();

private:
    void push(hjs::Value value);
    hjs::Value pop();

    std::shared_ptr<hjs::Upvalue> capture_upvalue(hjs::Value* local);
    void close_upvalues(size_t last_slot);

    struct ExceptionHandler {
        size_t frame_index;
        const uint8_t* catch_ip;
        size_t stack_depth;
    };
    std::vector<ExceptionHandler> m_exception_handlers;

    const hjs::vm::Chunk* m_chunk = nullptr;
    const uint8_t* m_ip = nullptr;
    std::vector<hjs::Value> m_stack;
    std::vector<std::shared_ptr<hjs::Upvalue>> m_open_upvalues;
};

} // namespace hjs::vm
