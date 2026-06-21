#include "rv32i/RV32IInstructionOracle.hpp"

#include "modules/composite/ALU32.hpp"
#include "modules/memory/BehavioralMemory64Kx32.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include <cstdint>

namespace rv32i {
namespace {
uint32_t addWrap(uint32_t a, uint32_t b) {
    return a + b;
}

uint32_t asUInt(int32_t value) {
    return static_cast<uint32_t>(value);
}

int32_t asInt(uint32_t value) {
    return static_cast<int32_t>(value);
}

uint32_t signExtend(uint32_t value, unsigned bits) {
    return static_cast<uint32_t>(RV32IDecoder::signExtend(value, bits));
}

size_t memorySizeBytes(RV32IMemorySize size) {
    switch (size) {
        case RV32IMemorySize::Byte: return 1;
        case RV32IMemorySize::Halfword: return 2;
        case RV32IMemorySize::Word: return 4;
        case RV32IMemorySize::None: return 0;
    }
    return 0;
}

bool isMisaligned(uint32_t address, RV32IMemorySize size) {
    switch (size) {
        case RV32IMemorySize::Byte: return false;
        case RV32IMemorySize::Halfword: return (address & 0x1U) != 0;
        case RV32IMemorySize::Word: return (address & 0x3U) != 0;
        case RV32IMemorySize::None: return true;
    }
    return true;
}

template<typename MemoryT>
RV32IExecutionTrapCause loadTrapCause(uint32_t address, RV32IMemorySize size, const MemoryT& memory) {
    if (isMisaligned(address, size)) {
        return RV32IExecutionTrapCause::LoadAddressMisaligned;
    }
    if (!memory.canAccess(address, memorySizeBytes(size))) {
        return RV32IExecutionTrapCause::LoadAccessFault;
    }
    return RV32IExecutionTrapCause::None;
}

template<typename MemoryT>
RV32IExecutionTrapCause storeTrapCause(uint32_t address, RV32IMemorySize size, const MemoryT& memory) {
    if (isMisaligned(address, size)) {
        return RV32IExecutionTrapCause::StoreAddressMisaligned;
    }
    if (!memory.canAccess(address, memorySizeBytes(size))) {
        return RV32IExecutionTrapCause::StoreAccessFault;
    }
    return RV32IExecutionTrapCause::None;
}

void trap(RV32IState& state, RV32IInstructionTrace& trace, RV32IExecutionTrapCause cause) {
    state.trapped = true;
    state.trap_cause = cause;
    trace.trapped = true;
    trace.trap_cause = cause;
    trace.pc_after = state.pc;
}

RV32IExecutionTrapCause mapControlTrap(RV32ITrapCause cause) {
    switch (cause) {
        case RV32ITrapCause::None: return RV32IExecutionTrapCause::None;
        case RV32ITrapCause::IllegalInstruction: return RV32IExecutionTrapCause::IllegalInstruction;
        case RV32ITrapCause::EnvironmentCall: return RV32IExecutionTrapCause::EnvironmentCall;
    }
    return RV32IExecutionTrapCause::IllegalInstruction;
}

uint32_t selectAluA(const RV32IControlSignals& control, const RV32IInstructionTrace& trace) {
    switch (control.alu_a) {
        case RV32IALUSourceA::RS1: return trace.rs1_value;
        case RV32IALUSourceA::PC: return trace.pc_before;
        case RV32IALUSourceA::Zero: return 0;
    }
    return 0;
}

uint32_t selectAluB(const RV32IControlSignals& control, const RV32IInstructionTrace& trace) {
    switch (control.alu_b) {
        case RV32IALUSourceB::RS2: return trace.rs2_value;
        case RV32IALUSourceB::Immediate: return asUInt(trace.immediate);
        case RV32IALUSourceB::Zero: return 0;
    }
    return 0;
}

uint32_t evaluateAlu(uint8_t op, uint32_t a, uint32_t b) {
    const auto shamt = b & 0x1FU;
    switch (op) {
        case ALU32Op::ADD: return addWrap(a, b);
        case ALU32Op::SUB: return a - b;
        case ALU32Op::AND: return a & b;
        case ALU32Op::OR: return a | b;
        case ALU32Op::XOR: return a ^ b;
        case ALU32Op::SLL: return a << shamt;
        case ALU32Op::SRL: return a >> shamt;
        case ALU32Op::SRA: return static_cast<uint32_t>(asInt(a) >> shamt);
        case ALU32Op::SLT: return asInt(a) < asInt(b) ? 1U : 0U;
        case ALU32Op::SLTU: return a < b ? 1U : 0U;
        case ALU32Op::PASS_A: return a;
        case ALU32Op::PASS_B: return b;
        case ALU32Op::ZERO: return 0;
        default: return 0;
    }
}

bool branchTaken(RV32IBranchType branch, uint32_t rs1, uint32_t rs2) {
    switch (branch) {
        case RV32IBranchType::None: return false;
        case RV32IBranchType::BEQ: return rs1 == rs2;
        case RV32IBranchType::BNE: return rs1 != rs2;
        case RV32IBranchType::BLT: return asInt(rs1) < asInt(rs2);
        case RV32IBranchType::BGE: return asInt(rs1) >= asInt(rs2);
        case RV32IBranchType::BLTU: return rs1 < rs2;
        case RV32IBranchType::BGEU: return rs1 >= rs2;
    }
    return false;
}

template<typename MemoryT>
uint32_t readMemoryValue(MemoryT& memory, RV32IMemorySize size, bool sign_extend, uint32_t address) {
    switch (size) {
        case RV32IMemorySize::Byte: {
            const auto value = memory.readU8(address);
            return sign_extend ? signExtend(value, 8) : value;
        }
        case RV32IMemorySize::Halfword: {
            const auto value = memory.readU16(address);
            return sign_extend ? signExtend(value, 16) : value;
        }
        case RV32IMemorySize::Word:
            return memory.readU32(address);
        case RV32IMemorySize::None:
            return 0;
    }
    return 0;
}

template<typename MemoryT>
void writeMemoryValue(MemoryT& memory, RV32IMemorySize size, uint32_t address, uint32_t value) {
    switch (size) {
        case RV32IMemorySize::Byte:
            memory.writeU8(address, static_cast<uint8_t>(value & 0xFFU));
            break;
        case RV32IMemorySize::Halfword:
            memory.writeU16(address, static_cast<uint16_t>(value & 0xFFFFU));
            break;
        case RV32IMemorySize::Word:
            memory.writeU32(address, value);
            break;
        case RV32IMemorySize::None:
            break;
    }
}

uint32_t writebackValue(const RV32IControlSignals& control,
                        const RV32IInstructionTrace& trace,
                        uint32_t memory_value) {
    switch (control.writeback) {
        case RV32IWritebackSource::None: return 0;
        case RV32IWritebackSource::ALU: return trace.alu_result;
        case RV32IWritebackSource::Memory: return memory_value;
        case RV32IWritebackSource::PCPlus4: return trace.pc_before + 4;
    }
    return 0;
}

class NonMutatingBehavioralMemory64Kx32 {
public:
    explicit NonMutatingBehavioralMemory64Kx32(BehavioralMemory64Kx32& memory)
        : memory_(memory) {}

    bool canAccess(uint32_t address, size_t count) const {
        return memory_.canAccess(address, count);
    }

    uint8_t readU8(uint32_t address) const { return memory_.readU8(address); }
    uint16_t readU16(uint32_t address) const { return memory_.readU16(address); }
    uint32_t readU32(uint32_t address) const { return memory_.readU32(address); }

    void writeU8(uint32_t address, uint8_t value) {
        (void)address;
        (void)value;
    }

    void writeU16(uint32_t address, uint16_t value) {
        (void)address;
        (void)value;
    }

    void writeU32(uint32_t address, uint32_t value) {
        (void)address;
        (void)value;
    }

private:
    BehavioralMemory64Kx32& memory_;
};

template<typename InstructionMemoryT, typename DataMemoryT>
RV32IInstructionTrace stepImpl(RV32IState& state,
                               InstructionMemoryT& instruction_memory,
                               DataMemoryT& data_memory) {
    state.forceX0();

    RV32IInstructionTrace trace;
    trace.instruction_index = state.instruction_count;
    trace.pc_before = state.pc;
    trace.pc_after = state.pc;
    trace.halted = state.halted;
    trace.trapped = state.trapped;
    trace.trap_cause = state.trap_cause;

    if (state.halted || state.trapped) {
        return trace;
    }

    if ((state.pc & 0x3U) != 0) {
        trap(state, trace, RV32IExecutionTrapCause::InstructionAddressMisaligned);
        ++state.instruction_count;
        return trace;
    }
    if (!instruction_memory.canAccess(state.pc, 4)) {
        trap(state, trace, RV32IExecutionTrapCause::InstructionAccessFault);
        ++state.instruction_count;
        return trace;
    }

    trace.raw_instruction = instruction_memory.readU32(state.pc);
    trace.decoded = RV32IDecoder::decode(trace.raw_instruction);
    trace.control = RV32IControl::fromDecoded(trace.decoded);

    trace.rs1 = trace.decoded.rs1;
    trace.rs2 = trace.decoded.rs2;
    trace.rd = trace.decoded.rd;
    trace.rs1_value = state.readRegister(trace.rs1);
    trace.rs2_value = state.readRegister(trace.rs2);
    trace.immediate = trace.decoded.immediate;
    trace.alu_a = selectAluA(trace.control, trace);
    trace.alu_b = selectAluB(trace.control, trace);
    trace.alu_result = evaluateAlu(trace.control.alu_op, trace.alu_a, trace.alu_b);

    if (trace.control.trap) {
        trap(state, trace, mapControlTrap(trace.control.trap_cause));
        ++state.instruction_count;
        return trace;
    }

    if (trace.control.halt) {
        state.halted = true;
        trace.halted = true;
        trace.pc_after = state.pc;
        ++state.instruction_count;
        return trace;
    }

    uint32_t memory_value = 0;
    if (trace.control.mem_read) {
        trace.memory.kind = RV32IMemoryAccessKind::Read;
        trace.memory.size = trace.control.mem_size;
        trace.memory.sign_extend = trace.control.load_sign_extend;
        trace.memory.address = trace.alu_result;
        const auto cause = loadTrapCause(trace.memory.address, trace.memory.size, data_memory);
        if (cause != RV32IExecutionTrapCause::None) {
            trace.memory.fault = true;
            trap(state, trace, cause);
            ++state.instruction_count;
            return trace;
        }
        memory_value = readMemoryValue(data_memory, trace.memory.size, trace.memory.sign_extend, trace.memory.address);
        trace.memory.read_data = memory_value;
    }

    if (trace.control.mem_write) {
        trace.memory.kind = RV32IMemoryAccessKind::Write;
        trace.memory.size = trace.control.mem_size;
        trace.memory.address = trace.alu_result;
        trace.memory.write_data = trace.rs2_value;
        const auto cause = storeTrapCause(trace.memory.address, trace.memory.size, data_memory);
        if (cause != RV32IExecutionTrapCause::None) {
            trace.memory.fault = true;
            trap(state, trace, cause);
            ++state.instruction_count;
            return trace;
        }
        writeMemoryValue(data_memory, trace.memory.size, trace.memory.address, trace.rs2_value);
    }

    if (trace.control.reg_write) {
        trace.writeback.enabled = true;
        trace.writeback.rd = trace.rd;
        trace.writeback.value = writebackValue(trace.control, trace, memory_value);
        trace.writeback.ignored_x0 = trace.rd == 0;
        state.writeRegister(trace.rd, trace.writeback.value);
    }

    uint32_t next_pc = state.pc + 4;
    if (trace.control.branch != RV32IBranchType::None) {
        trace.branch_taken = branchTaken(trace.control.branch, trace.rs1_value, trace.rs2_value);
        if (trace.branch_taken) {
            next_pc = state.pc + asUInt(trace.immediate);
        }
    } else if (trace.control.jump == RV32IJumpType::JAL) {
        next_pc = state.pc + asUInt(trace.immediate);
    } else if (trace.control.jump == RV32IJumpType::JALR) {
        next_pc = (trace.rs1_value + asUInt(trace.immediate)) & ~uint32_t{1};
    }

    state.pc = next_pc;
    state.forceX0();
    trace.pc_after = state.pc;
    trace.halted = state.halted;
    trace.trapped = state.trapped;
    trace.trap_cause = state.trap_cause;
    ++state.instruction_count;
    return trace;
}
}

uint32_t RV32IState::readRegister(uint8_t index) const {
    return index == 0 ? 0 : x[index & 0x1FU];
}

void RV32IState::writeRegister(uint8_t index, uint32_t value) {
    if ((index & 0x1FU) != 0) {
        x[index & 0x1FU] = value;
    }
    forceX0();
}

void RV32IState::forceX0() {
    x[0] = 0;
}

RV32IInstructionTrace RV32IInstructionOracle::step(RV32IState& state, RV32IFunctionalMemory& memory) {
    return step(state, memory, memory);
}

RV32IInstructionTrace RV32IInstructionOracle::step(RV32IState& state,
                                                   RV32IFunctionalMemory& instruction_memory,
                                                   RV32IFunctionalMemory& data_memory) {
    return stepImpl(state, instruction_memory, data_memory);
}

RV32IInstructionTrace RV32IInstructionOracle::stepWithoutDataMemoryWrite(
    RV32IState& state,
    BehavioralMemory64Kx32& instruction_memory,
    BehavioralMemory64Kx32& data_memory
) {
    NonMutatingBehavioralMemory64Kx32 non_mutating_data_memory(data_memory);
    return stepImpl(state, instruction_memory, non_mutating_data_memory);
}

RV32IInstructionRunResult RV32IInstructionOracle::run(RV32IState& state, RV32IFunctionalMemory& memory, size_t max_instructions) {
    RV32IInstructionRunResult result;

    while (!state.halted && !state.trapped && result.trace.size() < max_instructions) {
        result.trace.push_back(step(state, memory));
    }

    if (!state.halted && !state.trapped && result.trace.size() >= max_instructions) {
        state.trapped = true;
        state.trap_cause = RV32IExecutionTrapCause::InstructionLimit;
        result.instruction_limit_reached = true;
    }

    state.forceX0();
    result.final_state = state;
    return result;
}

std::string_view toString(RV32IExecutionTrapCause cause) {
    switch (cause) {
        case RV32IExecutionTrapCause::None: return "None";
        case RV32IExecutionTrapCause::IllegalInstruction: return "IllegalInstruction";
        case RV32IExecutionTrapCause::EnvironmentCall: return "EnvironmentCall";
        case RV32IExecutionTrapCause::InstructionAddressMisaligned: return "InstructionAddressMisaligned";
        case RV32IExecutionTrapCause::InstructionAccessFault: return "InstructionAccessFault";
        case RV32IExecutionTrapCause::LoadAddressMisaligned: return "LoadAddressMisaligned";
        case RV32IExecutionTrapCause::LoadAccessFault: return "LoadAccessFault";
        case RV32IExecutionTrapCause::StoreAddressMisaligned: return "StoreAddressMisaligned";
        case RV32IExecutionTrapCause::StoreAccessFault: return "StoreAccessFault";
        case RV32IExecutionTrapCause::InstructionLimit: return "InstructionLimit";
    }
    return "None";
}

std::string_view toString(RV32IMemoryAccessKind kind) {
    switch (kind) {
        case RV32IMemoryAccessKind::None: return "None";
        case RV32IMemoryAccessKind::Read: return "Read";
        case RV32IMemoryAccessKind::Write: return "Write";
    }
    return "None";
}

}
