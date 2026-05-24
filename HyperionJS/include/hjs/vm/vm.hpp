#pragma once

#include <hjs/vm/bytecode.hpp>
#include <hjs/vm/inline_cache.hpp>
#include <hjs/core/value.hpp>
#include <hjs/runtime/environment.hpp>
#include <hjs/runtime/object.hpp>
#include <hjs/gc/gc.hpp>
#include <hjs/debug/profiler.hpp>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <queue>

namespace hjs { class Upvalue; }

namespace hjs::vm {

struct Microtask {
    std::function<void()> task;
};

class VM {
public:
    enum class InterpretResult { Ok, CompileError, RuntimeError };

    InterpretResult interpret(const hjs::vm::Chunk& chunk);

    static hjs::runtime::Environment m_globals;

    static void setup_console();
    static void set_debug_mode(bool d) { s_debug_mode = d; }
    static bool debug_mode() { return s_debug_mode; }
    static void set_profile_mode(bool p) { s_profile_mode = p; }
    static bool profile_mode() { return s_profile_mode; }

    // Microtask queue for Promise resolution
    void enqueue_microtask(std::function<void()> task);
    void drain_microtask_queue();

    // GC
    static void try_gc();

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

    // ---- Inline Cache (polymorphic, 16-slot) ------------------------------
    struct ICKey {
        const void* object_ptr;
        size_t property_name_hash;
        bool operator==(const ICKey& o) const {
            return object_ptr == o.object_ptr && property_name_hash == o.property_name_hash;
        }
    };

    struct ICKeyHash {
        size_t operator()(const ICKey& k) const {
            return std::hash<const void*>()(k.object_ptr) ^ (k.property_name_hash << 5);
        }
    };

    static constexpr int PIC_SLOTS = 16;
    struct PICSlot {
        InlineCache ic;
    };

    std::unordered_map<ICKey, PICSlot, ICKeyHash> m_pic_cache;

    // ---- Microtask queue --------------------------------------------------
    std::queue<Microtask> m_microtask_queue;

    const hjs::vm::Chunk* m_chunk = nullptr;
    const uint8_t* m_ip = nullptr;
    std::vector<hjs::Value> m_stack;
    std::vector<std::shared_ptr<hjs::Upvalue>> m_open_upvalues;

    static bool s_debug_mode;
    static bool s_profile_mode;
};

} // namespace hjs::vm
