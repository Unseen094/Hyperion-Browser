#include <hjs/vm/jit_baseline.hpp>
#include <algorithm>
#include <cstring>

namespace hjs::vm {

// ---- Register Allocator --------------------------------------------------

RegisterAllocator::RegisterAllocator(int num_regs)
    : m_num_regs(num_regs)
    , m_phys_regs_used(num_regs, false)
{
}

int RegisterAllocator::allocate(int vreg, int current_ip) {
    auto it = m_vreg_to_phys.find(vreg);
    if (it != m_vreg_to_phys.end()) {
        return it->second;
    }

    for (int i = 0; i < m_num_regs; i++) {
        if (!m_phys_regs_used[i]) {
            m_phys_regs_used[i] = true;
            m_vreg_to_phys[vreg] = i;
            m_ranges.push_back({current_ip, current_ip + 1, vreg, i, false, 0});
            return i;
        }
    }

    int spill_reg = 0;
    int slot = m_next_spill++;
    m_vreg_to_spill[vreg] = slot;
    m_ranges.push_back({current_ip, current_ip + 1, vreg, spill_reg, true, slot});
    return spill_reg;
}

void RegisterAllocator::free_phys_reg(int phys_reg) {
    if (phys_reg >= 0 && phys_reg < m_num_regs) {
        m_phys_regs_used[phys_reg] = false;
    }
}

int RegisterAllocator::get_phys_reg(int vreg) const {
    auto it = m_vreg_to_phys.find(vreg);
    if (it != m_vreg_to_phys.end()) return it->second;
    return -1;
}

int RegisterAllocator::spill_slot(int vreg) const {
    auto it = m_vreg_to_spill.find(vreg);
    if (it != m_vreg_to_spill.end()) return it->second;
    return -1;
}

// ---- Optimizing JIT ------------------------------------------------------

OptimizingJIT::OptimizingJIT() = default;

OptimizingJIT& OptimizingJIT::instance() {
    static OptimizingJIT jit;
    return jit;
}

void OptimizingJIT::compile_function(std::shared_ptr<JSFunction> fn) {
    if (!fn || !fn->chunk()) return;

    CompiledStub stub;
    const auto& code = fn->chunk()->code();
    int local_count = fn->arity() + 8;
    RegisterAllocator reg_alloc(8);

    int max_spill = 0;
    int bytecode_offset = 0;
    int native_offset = 0;

    // First pass: count instructions for register allocation
    size_t ip = 0;
    while (ip < code.size()) {
        OpCode op = static_cast<OpCode>(code[ip]);
        switch (op) {
        case OpCode::GetLocal:
        case OpCode::SetLocal:
            ip += 2;
            break;
        case OpCode::Constant:
        case OpCode::GetProperty:
        case OpCode::SetProperty:
            ip += 2;
            break;
        case OpCode::Jump:
        case OpCode::JumpIfFalse:
        case OpCode::JumpIfTrue:
        case OpCode::Loop:
            ip += 3;
            break;
        case OpCode::Call:
            ip += 2;
            break;
        default:
            ip++;
            break;
        }
        bytecode_offset++;
    }

    int spill_count = reg_alloc.m_ranges.size();
    if (spill_count < 4) spill_count = 4;

    emit_prologue(stub.native_code, local_count, spill_count);

    ip = 0;
    while (ip < code.size()) {
        OpCode op = static_cast<OpCode>(code[ip]);
        int vreg_dst = -1, vreg_src1 = -1, vreg_src2 = -1;

        switch (op) {
        case OpCode::GetLocal: {
            uint8_t slot = code[ip + 1];
            int phys_reg = reg_alloc.allocate(slot, (int)ip);
            emit_register_load(stub.native_code, slot, phys_reg);
            vreg_dst = slot;
            ip += 2;
            break;
        }
        case OpCode::SetLocal: {
            uint8_t slot = code[ip + 1];
            int phys_reg = reg_alloc.allocate(slot, (int)ip);
            emit_register_store(stub.native_code, slot, phys_reg);
            ip += 2;
            break;
        }
        case OpCode::Constant: {
            ip += 2;
            break;
        }
        case OpCode::Add:
        case OpCode::Subtract:
        case OpCode::Multiply:
        case OpCode::Divide: {
            int r1 = reg_alloc.allocate(ip, (int)ip);
            int r2 = reg_alloc.allocate(ip + 1, (int)ip);
            int rd = reg_alloc.allocate(ip + 2, (int)ip);
            if (op == OpCode::Add) emit_unboxed_add(stub.native_code, rd, r1, r2);
            else if (op == OpCode::Subtract) emit_unboxed_sub(stub.native_code, rd, r1, r2);
            break;
        }
        case OpCode::GetProperty: {
            if (ip + 1 < code.size()) {
                int constant_idx = code[ip + 1];
                if (constant_idx >= 0 && constant_idx < (int)fn->chunk()->constants().size()) {
                    const auto& prop_name = fn->chunk()->constants()[constant_idx].as_string();
                    InlineCache ic;
                    emit_property_access_stub(stub.native_code, prop_name, ic);
                }
            }
            ip += 2;
            break;
        }
        case OpCode::SetProperty: {
            ip += 2;
            break;
        }
        case OpCode::Jump: {
            if (ip + 2 < code.size()) {
                uint16_t offset = ((uint16_t)code[ip + 1] << 8) | code[ip + 2];
                int placeholder = emit_placeholder_jump(stub.native_code);
                m_pending_patches.push_back({placeholder, (int)ip + (int)offset});
            }
            ip += 3;
            break;
        }
        case OpCode::JumpIfFalse:
        case OpCode::JumpIfTrue: {
            if (ip + 2 < code.size()) {
                uint16_t offset = ((uint16_t)code[ip + 1] << 8) | code[ip + 2];
                int placeholder = emit_placeholder_jump(stub.native_code);
                m_pending_patches.push_back({placeholder, (int)ip + (int)offset});
            }
            ip += 3;
            break;
        }
        case OpCode::Loop: {
            ip += 3;
            break;
        }
        case OpCode::Return: {
            emit_epilogue(stub.native_code);
            ip++;
            break;
        }
        case OpCode::Call: {
            ip += 2;
            break;
        }
        case OpCode::Nil:
        case OpCode::True:
        case OpCode::False:
        case OpCode::Pop:
        case OpCode::Dup:
        case OpCode::Print:
        case OpCode::Typeof:
        case OpCode::DefineGlobal:
        case OpCode::GetGlobal:
        case OpCode::SetGlobal:
        case OpCode::Negate:
        default: {
            ip++;
            break;
        }
        }
    }

    for (auto& patch : m_pending_patches) {
        patch_jump(stub.native_code, patch.placeholder_pos, patch.target_offset);
    }
    m_pending_patches.clear();

    stub.valid = true;
    m_compiled_map[fn.get()] = stub;
}

CompiledStub* OptimizingJIT::get_stub(std::shared_ptr<JSFunction> fn) {
    auto it = m_compiled_map.find(fn.get());
    if (it != m_compiled_map.end()) {
        it->second.use_count++;
        return &it->second;
    }
    return nullptr;
}

bool OptimizingJIT::is_compiled(std::shared_ptr<JSFunction> fn) const {
    return m_compiled_map.find(fn.get()) != m_compiled_map.end();
}

void OptimizingJIT::emit_prologue(std::vector<uint8_t>& code, int local_count, int spill_count) {
    (void)local_count;
    // push rbp
    code.push_back(0x55);
    // mov rbp, rsp
    code.push_back(0x48);
    code.push_back(0x89);
    code.push_back(0xE5);
    // sub rsp, spill_count * 8 + 32
    int frame_size = spill_count * 8 + 32;
    code.push_back(0x48);
    code.push_back(0x83);
    code.push_back(0xEC);
    code.push_back((uint8_t)frame_size);
}

void OptimizingJIT::emit_epilogue(std::vector<uint8_t>& code) {
    // add rsp, frame_size
    code.push_back(0x48);
    code.push_back(0x83);
    code.push_back(0xC4);
    code.push_back(0x40);
    // pop rbp
    code.push_back(0x5D);
    // ret
    code.push_back(0xC3);
}

void OptimizingJIT::emit_register_load(std::vector<uint8_t>& code, uint8_t slot, int phys_reg) {
    // mov reg, [rbp - offset]
    code.push_back(0x48);
    code.push_back(0x8B);
    code.push_back(0x45 | (phys_reg << 3));
    code.push_back((uint8_t)(-0x10 - (int)slot * 8));
}

void OptimizingJIT::emit_register_store(std::vector<uint8_t>& code, uint8_t slot, int phys_reg) {
    // mov [rbp - offset], reg
    code.push_back(0x48);
    code.push_back(0x89);
    code.push_back(0x45 | (phys_reg << 3));
    code.push_back((uint8_t)(-0x10 - (int)slot * 8));
}

void OptimizingJIT::emit_jump_link(std::vector<uint8_t>& code, int target_offset, int current_offset) {
    int32_t disp = target_offset - current_offset - 5;
    code.push_back(0xE9);
    code.push_back((uint8_t)(disp & 0xFF));
    code.push_back((uint8_t)((disp >> 8) & 0xFF));
    code.push_back((uint8_t)((disp >> 16) & 0xFF));
    code.push_back((uint8_t)((disp >> 24) & 0xFF));
}

void OptimizingJIT::emit_unboxed_add(std::vector<uint8_t>& code, int dst_reg, int src1_reg, int src2_reg) {
    // movsd xmm0, [rbp + src1_offset]  -- unboxed double load
    code.push_back(0xF2);
    code.push_back(0x0F);
    code.push_back(0x10);
    code.push_back(0x45 | (src1_reg << 3));
    code.push_back((uint8_t)(-0x10 - src1_reg * 8));
    // addsd xmm0, [rbp + src2_offset]
    code.push_back(0xF2);
    code.push_back(0x0F);
    code.push_back(0x58);
    code.push_back(0x45 | (src2_reg << 3));
    code.push_back((uint8_t)(-0x10 - src2_reg * 8));
    // movsd [rbp + dst_offset], xmm0
    code.push_back(0xF2);
    code.push_back(0x0F);
    code.push_back(0x11);
    code.push_back(0x45 | (dst_reg << 3));
    code.push_back((uint8_t)(-0x10 - dst_reg * 8));
}

void OptimizingJIT::emit_unboxed_sub(std::vector<uint8_t>& code, int dst_reg, int src1_reg, int src2_reg) {
    code.push_back(0xF2);
    code.push_back(0x0F);
    code.push_back(0x10);
    code.push_back(0x45 | (src1_reg << 3));
    code.push_back((uint8_t)(-0x10 - src1_reg * 8));
    code.push_back(0xF2);
    code.push_back(0x0F);
    code.push_back(0x5C);
    code.push_back(0x45 | (src2_reg << 3));
    code.push_back((uint8_t)(-0x10 - src2_reg * 8));
    code.push_back(0xF2);
    code.push_back(0x0F);
    code.push_back(0x11);
    code.push_back(0x45 | (dst_reg << 3));
    code.push_back((uint8_t)(-0x10 - dst_reg * 8));
}

void OptimizingJIT::emit_spill_store(std::vector<uint8_t>& code, int spill_slot, int phys_reg) {
    code.push_back(0x48);
    code.push_back(0x89);
    code.push_back(0x45);
    code.push_back((uint8_t)(-0x10 - (spill_slot + 8) * 8));
}

void OptimizingJIT::emit_spill_load(std::vector<uint8_t>& code, int spill_slot, int phys_reg) {
    code.push_back(0x48);
    code.push_back(0x8B);
    code.push_back(0x45);
    code.push_back((uint8_t)(-0x10 - (spill_slot + 8) * 8));
}

void OptimizingJIT::emit_property_access_stub(std::vector<uint8_t>& code, const std::wstring& name, const InlineCache& ic) {
    (void)name;
    (void)ic;
    code.push_back(0x90);
    code.push_back(0x90);
    code.push_back(0x90);
}

int OptimizingJIT::emit_placeholder_jump(std::vector<uint8_t>& code) {
    int pos = (int)code.size();
    code.push_back(0xE9);
    code.push_back(0x00);
    code.push_back(0x00);
    code.push_back(0x00);
    code.push_back(0x00);
    return pos;
}

void OptimizingJIT::patch_jump(std::vector<uint8_t>& code, int placeholder_pos, int target_offset) {
    if (placeholder_pos + 5 > (int)code.size()) return;
    int32_t disp = target_offset - placeholder_pos - 5;
    code[placeholder_pos + 1] = (uint8_t)(disp & 0xFF);
    code[placeholder_pos + 2] = (uint8_t)((disp >> 8) & 0xFF);
    code[placeholder_pos + 3] = (uint8_t)((disp >> 16) & 0xFF);
    code[placeholder_pos + 4] = (uint8_t)((disp >> 24) & 0xFF);
}

} // namespace hjs::vm
