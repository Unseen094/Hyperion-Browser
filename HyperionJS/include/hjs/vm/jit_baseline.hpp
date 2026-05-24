#pragma once

#include <hjs/vm/bytecode.hpp>
#include <hjs/vm/vm.hpp>
#include <hjs/runtime/object.hpp>
#include <hjs/vm/hidden_class.hpp>
#include <hjs/vm/inline_cache.hpp>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>

namespace hjs::vm {

struct JitStackFrame {
    const JSClosure* closure;
    const Chunk* chunk;
    const uint8_t* ip;
    size_t stack_start;
    double* local_registers;
    double return_value;
    bool has_return_value;
};

struct CompiledStub {
    std::vector<uint8_t> native_code;
    bool valid = false;
    int use_count = 0;
};

// Register allocator (linear scan)
struct LiveRange {
    int start;
    int end;
    int vreg;
    int phys_reg = -1;
    bool spilled = false;
    int spill_slot = 0;
};

class RegisterAllocator {
public:
    RegisterAllocator(int num_regs = 8);

    int allocate(int vreg, int current_ip);
    void free_phys_reg(int phys_reg);
    int get_phys_reg(int vreg) const;
    int spill_slot(int vreg) const;

    std::vector<LiveRange> m_ranges;

private:
    int m_num_regs;
    int m_next_spill = 0;
    std::vector<bool> m_phys_regs_used;
    std::unordered_map<int, int> m_vreg_to_phys;
    std::unordered_map<int, int> m_vreg_to_spill;
};

class OptimizingJIT {
public:
    OptimizingJIT();

    void compile_function(std::shared_ptr<JSFunction> fn);
    CompiledStub* get_stub(std::shared_ptr<JSFunction> fn);

    bool is_compiled(std::shared_ptr<JSFunction> fn) const;
    size_t compiled_count() const { return m_compiled_map.size(); }

    static OptimizingJIT& instance();

private:
    void emit_prologue(std::vector<uint8_t>& code, int local_count, int spill_count);
    void emit_epilogue(std::vector<uint8_t>& code);
    void emit_register_load(std::vector<uint8_t>& code, uint8_t slot, int phys_reg);
    void emit_register_store(std::vector<uint8_t>& code, uint8_t slot, int phys_reg);
    void emit_jump_link(std::vector<uint8_t>& code, int target_offset, int current_offset);
    void emit_property_access_stub(std::vector<uint8_t>& code, const std::wstring& name, const InlineCache& ic);
    void emit_unboxed_add(std::vector<uint8_t>& code, int dst_reg, int src1_reg, int src2_reg);
    void emit_unboxed_sub(std::vector<uint8_t>& code, int dst_reg, int src1_reg, int src2_reg);
    void emit_spill_store(std::vector<uint8_t>& code, int spill_slot, int phys_reg);
    void emit_spill_load(std::vector<uint8_t>& code, int spill_slot, int phys_reg);
    int emit_placeholder_jump(std::vector<uint8_t>& code);
    void patch_jump(std::vector<uint8_t>& code, int placeholder_pos, int target_offset);

    std::unordered_map<JSFunction*, CompiledStub> m_compiled_map;

    struct JumpPatch {
        int placeholder_pos;
        int target_offset;
    };
    std::vector<JumpPatch> m_pending_patches;
};

// Keep the old name for backward compat
using BaselineJIT = OptimizingJIT;

} // namespace hjs::vm
