#pragma once
// The pipeline's stage rules as plain functions on values. The behavioral
// form of each stage and the behavioral form of the whole core both call
// these, so each rule is stated once. (The instruction oracle in src/rv32i
// keeps its own independent statement on purpose.)

#include "modules/composite/ALU32.hpp"
#include "modules/composite/ALU32Behavioral.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>

namespace rv32i::pipeline_rules {

inline bool dataMisaligned(uint32_t address, RV32IMemorySize size) {
    switch (size) {
        case RV32IMemorySize::Byte: return false;
        case RV32IMemorySize::Halfword: return (address & 1U) != 0;
        case RV32IMemorySize::Word: return (address & 3U) != 0;
        case RV32IMemorySize::None: return true;
    }
    return true;
}

inline RV32IExecutionTrapCause decodeTrap(RV32ITrapCause cause) {
    switch (cause) {
        case RV32ITrapCause::None: return RV32IExecutionTrapCause::None;
        case RV32ITrapCause::IllegalInstruction: return RV32IExecutionTrapCause::IllegalInstruction;
        case RV32ITrapCause::EnvironmentCall: return RV32IExecutionTrapCause::EnvironmentCall;
    }
    // A code decode never produces passes through unchanged, as in the gates.
    return static_cast<RV32IExecutionTrapCause>(cause);
}

inline uint32_t writebackValue(const RV32IControlSignals& control, uint32_t alu_result,
                               uint32_t memory_data, uint32_t pc_plus_4) {
    switch (control.writeback) {
        case RV32IWritebackSource::ALU: return alu_result;
        case RV32IWritebackSource::Memory: return memory_data;
        case RV32IWritebackSource::PCPlus4: return pc_plus_4;
        case RV32IWritebackSource::None: return 0;
    }
    return 0;
}

// An instruction that must stop before it reaches memory: a fetch problem,
// an illegal, trapping or halting instruction, or a misaligned jump target.
inline bool preterminal(bool pc_misaligned, bool instruction_fault,
                        const RV32IControlSignals& control, bool target_misaligned) {
    return pc_misaligned || instruction_fault || !control.legal || control.trap
        || control.halt || target_misaligned;
}

// Why a retiring instruction traps, oldest cause first.
inline RV32IExecutionTrapCause retirementCause(
    bool valid, bool pc_misaligned, bool instruction_fault, const RV32IControlSignals& control,
    bool data_misaligned, bool data_fault, bool target_misaligned) {
    using Cause = RV32IExecutionTrapCause;
    if (!valid) return Cause::None;
    if (pc_misaligned) return Cause::InstructionAddressMisaligned;
    if (instruction_fault) return Cause::InstructionAccessFault;
    if (!control.legal) return Cause::IllegalInstruction;
    if (control.trap) return decodeTrap(control.trap_cause);
    if (data_misaligned) return control.mem_write ? Cause::StoreAddressMisaligned : Cause::LoadAddressMisaligned;
    if (data_fault) return control.mem_write ? Cause::StoreAccessFault : Cause::LoadAccessFault;
    if (target_misaligned) return Cause::InstructionAddressMisaligned;
    return Cause::None;
}

// One producer an operand may be forwarded from.
struct ForwardSource {
    bool valid = false, reg_write = false, ready = true;
    uint8_t rd = 0;
    uint32_t value = 0;
};

// The newest matching producer wins. A younger producer whose value is not
// ready yet (a load still in memory) hides the older writeback value.
inline uint32_t forwardOperand(uint8_t source, uint32_t original,
                               const ForwardSource& ex_mem, const ForwardSource& mem_wb) {
    if (source == 0) return original;
    if (ex_mem.valid && ex_mem.reg_write && ex_mem.rd == source) return ex_mem.ready ? ex_mem.value : original;
    if (mem_wb.valid && mem_wb.reg_write && mem_wb.rd == source) return mem_wb.value;
    return original;
}

struct ExecuteResult {
    uint32_t alu_result = 0, pc_plus_4 = 0, next_pc = 0, target = 0;
    bool is_branch = false, branch_taken = false, control_taken = false, target_misaligned = false;
};

// The execute stage: choose the ALU operands, compute the result, and decide
// where control goes next. Branch conditions come from the ALU's comparison
// of its two operands, as in the structural stage.
inline ExecuteResult execute(const RV32IControlSignals& control, uint32_t pc, uint32_t immediate,
                             uint32_t rs1, uint32_t rs2) {
    ExecuteResult r;
    const uint32_t a = control.alu_a == RV32IALUSourceA::RS1 ? rs1 : control.alu_a == RV32IALUSourceA::PC ? pc : 0U;
    const uint32_t b = control.alu_b == RV32IALUSourceB::RS2 ? rs2
                     : control.alu_b == RV32IALUSourceB::Immediate ? immediate : 0U;
    r.alu_result = alu32Result(control.alu_op, a, b);
    r.pc_plus_4 = pc + 4U;
    const bool compare = control.alu_op == ALU32Op::SUB || control.alu_op == ALU32Op::SLT
                      || control.alu_op == ALU32Op::SLTU;
    const bool eq = compare && a == b, lt = compare && int32_t(a) < int32_t(b), ltu = compare && a < b;
    const bool conditions[]{false, eq, !eq, lt, !lt, ltu, !ltu, false};
    r.is_branch = control.branch != RV32IBranchType::None;
    r.branch_taken = conditions[unsigned(control.branch) & 7];
    const bool jump = control.jump != RV32IJumpType::None;
    r.target = !jump ? pc + immediate
             : control.jump == RV32IJumpType::JAL ? r.alu_result
             : control.jump == RV32IJumpType::JALR ? r.alu_result & ~uint32_t{1} : 0;
    r.control_taken = r.branch_taken || jump;
    r.next_pc = r.control_taken ? r.target : r.pc_plus_4;
    r.target_misaligned = r.control_taken && (r.next_pc & 3U) != 0;
    return r;
}

} // namespace rv32i::pipeline_rules
