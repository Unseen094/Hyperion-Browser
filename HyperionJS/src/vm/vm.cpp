#include <hjs/vm/vm.hpp>
#include <hjs/runtime/environment.hpp>
#include <hjs/runtime/object.hpp>
#include <hjs/runtime/stdlib.hpp>
#include <iostream>
#include <cmath>

namespace hjs::vm {

runtime::Environment VM::m_globals;

struct CallFrame {
    const JSClosure* closure;
    const Chunk* chunk;
    const uint8_t* ip;
    size_t stack_start;
};

// ---- Value helpers -------------------------------------------------------

static bool values_equal(const Value& a, const Value& b) {
    if (a.is_null() && b.is_null()) return true;
    if (a.is_boolean() && b.is_boolean()) return a.as_boolean() == b.as_boolean();
    if (a.is_number() && b.is_number()) return a.as_number() == b.as_number();
    if (a.is_string() && b.is_string()) return a.as_string() == b.as_string();
    if (a.is_object() && b.is_object()) return a.as_object() == b.as_object();
    return false;
}

static bool value_truthy(const Value& v) {
    if (v.is_null()) return false;
    if (v.is_boolean()) return v.as_boolean();
    if (v.is_number()) return v.as_number() != 0.0;
    if (v.is_string()) return !v.as_string().empty();
    return true; // objects are truthy
}

static Value add_values(const Value& a, const Value& b) {
    if (a.is_number() && b.is_number())
        return Value(a.as_number() + b.as_number());
    // String concatenation
    return Value(a.to_string() + b.to_string());
}

static std::wstring typeof_value(const Value& v) {
    if (v.is_null())    return L"undefined";
    if (v.is_boolean()) return L"boolean";
    if (v.is_number())  return L"number";
    if (v.is_string())  return L"string";
    if (v.is_object()) {
        if (dynamic_cast<JSFunction*>(v.as_object().get())) return L"function";
        if (dynamic_cast<NativeFunction*>(v.as_object().get())) return L"function";
        return L"object";
    }
    return L"undefined";
}

// ---- Interpreter ---------------------------------------------------------

VM::InterpretResult VM::interpret(const Chunk& chunk) {
    // Initialize standard library on first call
    static bool stdlib_initialized = false;
    if (!stdlib_initialized) {
        runtime::StdLib::initialize();
        setup_console();
        stdlib_initialized = true;
    }

    m_chunk = &chunk;
    m_ip    = chunk.code().data();

    // Create a top-level closure for the main script
    auto script_function = std::make_shared<JSFunction>(L"main", 0, m_chunk);
    auto script_closure = std::make_shared<JSClosure>(script_function);

    std::vector<CallFrame> frames;
    frames.push_back(CallFrame{ script_closure.get(), m_chunk, m_ip, 0 });

    // ---- Helper lambdas -------------------------------------------------
    auto read_byte = [&]() -> uint8_t { return *m_ip++; };

    auto read_constant = [&]() -> Value {
        return m_chunk->constants()[read_byte()];
    };

    auto read_u16 = [&]() -> uint16_t {
        uint16_t hi = read_byte();
        uint16_t lo = read_byte();
        return (hi << 8) | lo;
    };

    // ---- Main loop ------------------------------------------------------
    for (;;) {
        // Bounds check: ensure IP is within chunk bounds
        size_t offset = m_ip - m_chunk->code().data();
        if (offset >= m_chunk->code().size()) {
            std::wcout << L"VM Error: Instruction pointer out of bounds" << std::endl;
            return InterpretResult::RuntimeError;
        }

        uint8_t instruction = read_byte();

        switch ((OpCode)instruction) {

        // ---- Constants --------------------------------------------------
        case OpCode::Constant: push(read_constant()); break;
        case OpCode::Nil:      push(Value());          break;
        case OpCode::True:     push(Value(true));      break;
        case OpCode::False:    push(Value(false));     break;

        // ---- Arithmetic -------------------------------------------------
        case OpCode::Add: {
            Value b = pop(); Value a = pop();
            push(add_values(a, b));
            break;
        }
        case OpCode::Subtract: { double b = pop().as_number(); push(Value(pop().as_number() - b)); break; }
        case OpCode::Multiply: { double b = pop().as_number(); push(Value(pop().as_number() * b)); break; }
        case OpCode::Divide:   { double b = pop().as_number(); push(Value(pop().as_number() / b)); break; }
        case OpCode::Modulo:   { double b = pop().as_number(); push(Value(std::fmod(pop().as_number(), b))); break; }
        case OpCode::Negate:   { push(Value(-pop().as_number())); break; }

        // ---- Comparison -------------------------------------------------
        case OpCode::Equal:          { Value b = pop(); Value a = pop(); push(Value(values_equal(a, b))); break; }
        case OpCode::StrictEqual:    { Value b = pop(); Value a = pop(); push(Value(values_equal(a, b))); break; }
        case OpCode::NotEqual:       { Value b = pop(); Value a = pop(); push(Value(!values_equal(a, b))); break; }
        case OpCode::StrictNotEqual: { Value b = pop(); Value a = pop(); push(Value(!values_equal(a, b))); break; }
        case OpCode::Less:        { double b = pop().as_number(); push(Value(pop().as_number() <  b)); break; }
        case OpCode::LessEqual:   { double b = pop().as_number(); push(Value(pop().as_number() <= b)); break; }
        case OpCode::Greater:     { double b = pop().as_number(); push(Value(pop().as_number() >  b)); break; }
        case OpCode::GreaterEqual:{ double b = pop().as_number(); push(Value(pop().as_number() >= b)); break; }

        // ---- Logical ----------------------------------------------------
        case OpCode::Not: { push(Value(!value_truthy(pop()))); break; }

        // ---- Bitwise ----------------------------------------------------
        case OpCode::BitAnd: { long long b = (long long)pop().as_number(); push(Value((double)((long long)pop().as_number() & b))); break; }
        case OpCode::BitOr:  { long long b = (long long)pop().as_number(); push(Value((double)((long long)pop().as_number() | b))); break; }
        case OpCode::BitXor: { long long b = (long long)pop().as_number(); push(Value((double)((long long)pop().as_number() ^ b))); break; }

        // ---- Stack ------------------------------------------------------
        case OpCode::Pop: pop(); break;
        case OpCode::Dup: { if (!m_stack.empty()) { Value v = m_stack.back(); push(v); } break; }
        case OpCode::Print: { std::wcout << pop().to_string() << L"\n"; break; }

        // ---- Typeof -----------------------------------------------------
        case OpCode::Typeof: { push(Value(typeof_value(pop()))); break; }

        // ---- Variables --------------------------------------------------
        case OpCode::DefineGlobal: {
            std::wstring name = read_constant().as_string();
            m_globals.define(name, pop());
            break;
        }
        case OpCode::GetGlobal: {
            std::wstring name = read_constant().as_string();
            Value* v = m_globals.get(name);
            push(v ? *v : Value());
            break;
        }
        case OpCode::SetGlobal: {
            std::wstring name = read_constant().as_string();
            Value top = m_stack.back(); // don't pop — assignment is expression
            m_globals.assign(name, top);
            break;
        }
        case OpCode::GetLocal: {
            uint8_t slot = read_byte();
            push(m_stack[frames.back().stack_start + slot]);
            break;
        }
        case OpCode::SetLocal: {
            uint8_t slot = read_byte();
            m_stack[frames.back().stack_start + slot] = m_stack.back();
            break;
        }
        case OpCode::GetUpvalue: {
            uint8_t slot = read_byte();
            push(*frames.back().closure->upvalues()[slot]->value());
            break;
        }
        case OpCode::SetUpvalue: {
            uint8_t slot = read_byte();
            *frames.back().closure->upvalues()[slot]->value() = m_stack.back();
            break;
        }

        // ---- Control flow (2-byte offsets) ------------------------------
        case OpCode::Jump: {
            uint16_t offset = read_u16();
            m_ip += offset;
            break;
        }
        case OpCode::JumpIfFalse: {
            uint16_t offset = read_u16();
            if (!value_truthy(m_stack.back())) m_ip += offset;
            break;
        }
        case OpCode::JumpIfTrue: {
            uint16_t offset = read_u16();
            if (value_truthy(m_stack.back())) m_ip += offset;
            break;
        }
        case OpCode::Loop: {
            uint16_t offset = read_u16();
            m_ip -= offset;
            break;
        }

        // ---- Exceptions -------------------------------------------------
        case OpCode::TryStart: {
            uint16_t catch_offset = read_u16();
            m_exception_handlers.push_back({
                frames.size() - 1,
                m_ip + catch_offset,
                m_stack.size()
            });
            break;
        }
        case OpCode::TryEnd: {
            if (!m_exception_handlers.empty()) {
                m_exception_handlers.pop_back();
            }
            break;
        }
        case OpCode::Throw: {
            Value ex = pop();
            if (m_exception_handlers.empty()) {
                std::wcout << L"Uncaught Exception: " << ex.to_string() << L"\n";
                return InterpretResult::RuntimeError;
            }
            // Unwind to catch block
            auto handler = m_exception_handlers.back();
            m_exception_handlers.pop_back();
            
            // Restore frame and IP
            while (frames.size() - 1 > handler.frame_index) frames.pop_back();
            m_chunk = frames.back().chunk;
            m_ip = handler.catch_ip;

            // Unwind stack
            while (m_stack.size() > handler.stack_depth) pop();
            
            // Push the thrown exception for the catch block to use
            push(ex);
            break;
        }
        
        case OpCode::Closure: {
            auto fn_val = read_constant();
            auto fn = std::dynamic_pointer_cast<JSFunction>(fn_val.as_object());
            auto closure = std::make_shared<JSClosure>(fn);
            
            // Capture upvalues
            for (int i = 0; i < (int)fn->upvalue_count(); i++) {
                uint8_t is_local = read_byte();
                uint8_t index    = read_byte();
                if (is_local) {
                    closure->upvalues().push_back(capture_upvalue(m_stack.data() + frames.back().stack_start + index));
                } else {
                    closure->upvalues().push_back(frames.back().closure->upvalues()[index]);
                }
            }
            
            push(Value(closure));
            break;
        }

        // ---- Return -----------------------------------------------------
        case OpCode::Return: {
            Value result = pop();
            size_t frame_start = frames.back().stack_start;
            frames.pop_back();

            if (frames.empty()) {
                return InterpretResult::Ok;
            }

            // Unwind stack to caller frame
            while (m_stack.size() > frame_start) pop();

            m_chunk = frames.back().chunk;
            m_ip    = frames.back().ip;
            
            // Close upvalues that are going out of scope
            close_upvalues(frame_start);
            
            push(std::move(result));
            break;
        }

        // ---- Call -------------------------------------------------------
        case OpCode::Call: {
            int arg_count = read_byte();
            size_t callee_slot = m_stack.size() - arg_count - 1;
            Value callee_val = m_stack[callee_slot];

            if (callee_val.is_object()) {
                auto obj = callee_val.as_object();

                // Bound function — extract receiver + method
                if (auto* bound = dynamic_cast<JSBoundFunction*>(obj.get())) {
                    m_stack[callee_slot] = bound->receiver();
                    obj = bound->method().as_object();
                } else {
                    m_stack[callee_slot] = Value(); // this = undefined
                }

                // Closure call
                if (auto* target_closure = dynamic_cast<JSClosure*>(obj.get())) {
                    auto fn = target_closure->function();
                    frames.back().ip = m_ip;
                    frames.push_back(CallFrame{ target_closure, fn->chunk(), fn->chunk()->code().data(), callee_slot });
                    m_chunk = fn->chunk();
                    m_ip    = fn->chunk()->code().data();
                    continue;
                }

                // JS function call (legacy/direct)
                if (auto* target_fn = dynamic_cast<JSFunction*>(obj.get())) {
                    auto fn_copy = std::make_shared<JSFunction>(target_fn->name(), target_fn->arity(), target_fn->chunk());
                    fn_copy->set_upvalue_count(target_fn->upvalue_count());
                    auto temp_closure = std::make_shared<JSClosure>(fn_copy);
                    frames.back().ip = m_ip;
                    frames.push_back(CallFrame{ temp_closure.get(), target_fn->chunk(), target_fn->chunk()->code().data(), callee_slot });
                    m_chunk = target_fn->chunk();
                    m_ip    = target_fn->chunk()->code().data();
                    continue;
                }

                // Native function call
                if (auto* nat = dynamic_cast<NativeFunction*>(obj.get())) {
                    Value receiver = m_stack[callee_slot];
                    Value* args    = m_stack.data() + callee_slot + 1;
                    Value result   = nat->function()(receiver, arg_count, args);
                    for (int i = 0; i <= arg_count; i++) pop();
                    push(std::move(result));
                    break;
                }
            }
            // Not callable — push nil
            for (int i = 0; i <= arg_count; i++) pop();
            push(Value());
            break;
        }

        case OpCode::CallNew: {
            int arg_count = read_byte();
            size_t callee_slot = m_stack.size() - arg_count - 1;
            Value callee_val = m_stack[callee_slot];

            auto instance = std::make_shared<JSObject>();

            if (callee_val.is_object()) {
                auto class_obj = callee_val.as_object();
                // Inherit prototype
                Value* proto = class_obj->get_property(L"prototype");
                if (proto && proto->is_object()) {
                    instance->set_prototype(proto->as_object());
                }
                // Call constructor if present
                Value* ctor_val = class_obj->get_property(L"constructor");
                if (ctor_val && ctor_val->is_object()) {
                    if (auto* target_fn = dynamic_cast<JSFunction*>(ctor_val->as_object().get())) {
                        auto fn_copy = std::make_shared<JSFunction>(target_fn->name(), target_fn->arity(), target_fn->chunk());
                        fn_copy->set_upvalue_count(target_fn->upvalue_count());
                        auto temp_closure = std::make_shared<JSClosure>(fn_copy);
                        m_stack[callee_slot] = Value(instance);
                        frames.back().ip = m_ip;
                        frames.push_back(CallFrame{ temp_closure.get(), target_fn->chunk(), target_fn->chunk()->code().data(), callee_slot });
                        m_chunk = target_fn->chunk();
                        m_ip    = target_fn->chunk()->code().data();
                        continue;
                    }
                }
            }

            // No constructor — just return instance
            for (int i = 0; i <= arg_count; i++) pop();
            push(Value(instance));
            break;
        }

        // ---- Objects & arrays -------------------------------------------
        case OpCode::GetProperty: {
            std::wstring name = read_constant().as_string();
            Value obj_val = pop();
            if (obj_val.is_object()) {
                auto obj = obj_val.as_object();
                Value* val = obj->get_property(name);
                if (val) {
                    bool is_callable = val->is_object() &&
                        (dynamic_cast<JSFunction*>(val->as_object().get()) ||
                         dynamic_cast<NativeFunction*>(val->as_object().get()));
                    push(is_callable
                        ? Value(std::make_shared<JSBoundFunction>(obj_val, *val))
                        : *val);
                } else {
                    push(Value());
                }
            } else if (obj_val.is_string()) {
                // String properties: length + prototype methods
                if (name == L"length") {
                    push(Value((double)obj_val.as_string().size()));
                } else if (runtime::StdLib::string_prototype) {
                    Value* val = runtime::StdLib::string_prototype->get_property(name);
                    if (val) push(Value(std::make_shared<JSBoundFunction>(obj_val, *val)));
                    else push(Value());
                } else {
                    push(Value());
                }
            } else {
                push(Value());
            }
            break;
        }
        case OpCode::SetProperty: {
            std::wstring name = read_constant().as_string();
            Value value   = pop();
            Value obj_val = pop();
            if (obj_val.is_object()) obj_val.as_object()->set_property(name, value);
            push(value);
            break;
        }
        case OpCode::CreateObject: {
            int count = read_byte();
            auto obj = std::make_shared<JSObject>();
            for (int i = 0; i < count; ++i) {
                std::wstring name = read_constant().as_string();
                obj->set_property(name, pop());
            }
            push(Value(obj));
            break;
        }
        case OpCode::CreateArray: {
            int count = read_byte();
            auto arr = std::make_shared<JSArray>();
            arr->elements.resize(count);
            for (int i = count - 1; i >= 0; --i) arr->elements[i] = pop();
            arr->set_property(L"length", Value((double)count));
            if (runtime::StdLib::array_prototype)
                arr->set_prototype(runtime::StdLib::array_prototype);
            push(Value(arr));
            break;
        }
        case OpCode::GetIndex: {
            Value idx = pop(); Value obj_val = pop();
            if (obj_val.is_object()) {
                auto obj = obj_val.as_object();
                if (idx.is_number()) {
                    if (auto* arr = dynamic_cast<JSArray*>(obj.get())) {
                        int i = (int)idx.as_number();
                        Value* v = arr->get_index(i);
                        push(v ? *v : Value()); break;
                    }
                }
                if (idx.is_string()) {
                    Value* v = obj->get_property(idx.as_string());
                    push(v ? *v : Value()); break;
                }
            } else if (obj_val.is_string() && idx.is_number()) {
                const auto& s = obj_val.as_string();
                int i = (int)idx.as_number();
                if (i >= 0 && i < (int)s.size())
                    push(Value(std::wstring(1, s[i])));
                else
                    push(Value());
                break;
            }
            push(Value());
            break;
        }
        case OpCode::SetIndex: {
            Value value = pop(); Value idx = pop(); Value obj_val = pop();
            if (obj_val.is_object()) {
                auto obj = obj_val.as_object();
                if (idx.is_number()) {
                    if (auto* arr = dynamic_cast<JSArray*>(obj.get()))
                        arr->set_index((int)idx.as_number(), value);
                } else if (idx.is_string()) {
                    obj->set_property(idx.as_string(), value);
                }
            }
            push(value);
            break;
        }

        // Unhandled opcodes — report error
        default:
            std::wcout << L"VM Error: Unknown opcode 0x" << std::hex << (int)instruction << std::dec << std::endl;
            return InterpretResult::RuntimeError;
        }
    }
}

// ---- console object setup -----------------------------------------------

void VM::setup_console() {
    auto console = std::make_shared<JSObject>();

    NativeFn log_fn = [](Value, int argc, Value* args) -> Value {
        for (int i = 0; i < argc; i++) {
            if (i) std::wcout << L" ";
            std::wcout << args[i].to_string();
        }
        std::wcout << L"\n";
        return Value();
    };

    NativeFn warn_fn = [](Value, int argc, Value* args) -> Value {
        std::wcout << L"[warn] ";
        for (int i = 0; i < argc; i++) { if (i) std::wcout << L" "; std::wcout << args[i].to_string(); }
        std::wcout << L"\n";
        return Value();
    };

    NativeFn error_fn = [](Value, int argc, Value* args) -> Value {
        std::wcout << L"[error] ";
        for (int i = 0; i < argc; i++) { if (i) std::wcout << L" "; std::wcout << args[i].to_string(); }
        std::wcout << L"\n";
        return Value();
    };

    console->set_property(L"log",   Value(std::make_shared<NativeFunction>(log_fn)));
    console->set_property(L"warn",  Value(std::make_shared<NativeFunction>(warn_fn)));
    console->set_property(L"error", Value(std::make_shared<NativeFunction>(error_fn)));

    m_globals.define(L"console", Value(console));
}

void VM::push(Value value) { m_stack.push_back(std::move(value)); }
Value VM::pop() {
    if (m_stack.empty()) return Value();
    Value v = std::move(m_stack.back());
    m_stack.pop_back();
    return v;
}

std::shared_ptr<Upvalue> VM::capture_upvalue(Value* local) {
    for (auto& uv : m_open_upvalues) {
        if (uv->value() == local) return uv;
    }
    
    auto uv = std::make_shared<Upvalue>(local);
    m_open_upvalues.push_back(uv);
    return uv;
}

void VM::close_upvalues(size_t last_slot) {
    auto it = m_open_upvalues.begin();
    while (it != m_open_upvalues.end()) {
        auto& uv = *it;
        Value* uv_ptr = uv->value();
        Value* stack_start = m_stack.data() + last_slot;
        // Only close if pointer is within the stack range being unwound
        if (uv_ptr >= stack_start && uv_ptr < m_stack.data() + m_stack.size()) {
            uv->close();
            it = m_open_upvalues.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace hjs::vm
