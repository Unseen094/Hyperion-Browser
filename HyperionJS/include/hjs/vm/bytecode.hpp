#pragma once

#include <vector>
#include <cstdint>
#include <hjs/core/value.hpp>

namespace hjs::vm {

enum class OpCode : uint8_t {
    // Constants & literals
    Constant,
    Nil,
    True,
    False,

    // Arithmetic
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Negate,

    // Comparison
    Equal,       // ==  (loose)
    StrictEqual, // ===
    NotEqual,
    StrictNotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    // Logical
    Not,
    And,         // short-circuit (jump-based)
    Or,          // short-circuit (jump-based)

    // Bitwise
    BitAnd,
    BitOr,
    BitXor,
    ShiftLeft,
    ShiftRight,

    // Increment / Decrement (pre/post handled by compiler)
    Increment,
    Decrement,

    // Variables
    DefineGlobal,
    GetGlobal,
    SetGlobal,
    GetLocal,
    SetLocal,
    GetUpvalue,
    SetUpvalue,

    // Stack ops
    Pop,
    Dup,

    // Control flow
    Jump,        // unconditional (2-byte offset, big-endian)
    JumpIfFalse, // conditional   (2-byte offset, big-endian)
    JumpIfTrue,  // for ||        (2-byte offset)
    Loop,        // backward jump (2-byte offset)

    // Calls & returns
    Call,
    CallNew,     // new Foo(...)
    Return,

    // Objects & arrays
    GetProperty,
    SetProperty,
    CreateObject,
    CreateArray,
    GetIndex,
    SetIndex,

    // I/O (temp, mirrors print)
    Print,

    // Typeof
    Typeof,

    // Exceptions
    Throw,
    TryStart, // 2-byte jump offset to catch block
    TryEnd,
    Closure,
    Await,
};

// ---- Chunk -----------------------------------------------------------------

class Chunk {
public:
    void write(uint8_t byte) {
        m_code.push_back(byte);
    }

    void write16(uint16_t value) {
        m_code.push_back((uint8_t)(value >> 8));
        m_code.push_back((uint8_t)(value & 0xFF));
    }

    int add_constant(hjs::Value value) {
        m_constants.push_back(std::move(value));
        return (int)m_constants.size() - 1;
    }

    // Patch a 2-byte big-endian jump offset at code position `pos`
    void patch16(int pos, uint16_t value) {
        m_code[pos]     = (uint8_t)(value >> 8);
        m_code[pos + 1] = (uint8_t)(value & 0xFF);
    }

    int current_offset() const { return (int)m_code.size(); }

    const std::vector<uint8_t>&  code()      const { return m_code; }
    const std::vector<hjs::Value>& constants() const { return m_constants; }

private:
    std::vector<uint8_t>      m_code;
    std::vector<hjs::Value>   m_constants;
};

} // namespace hjs::vm
