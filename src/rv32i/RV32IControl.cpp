#include "rv32i/RV32IControl.hpp"

#include "modules/composite/ALU32.hpp"

namespace rv32i {
namespace {
RV32IControlSignals baseLegal(uint8_t alu_op,
                              RV32IALUSourceA alu_a,
                              RV32IALUSourceB alu_b) {
    RV32IControlSignals control;
    control.legal = true;
    control.alu_op = alu_op;
    control.alu_a = alu_a;
    control.alu_b = alu_b;
    return control;
}

RV32IControlSignals trapControl(RV32ITrapCause cause) {
    RV32IControlSignals control;
    control.alu_op = ALU32Op::ZERO;
    control.trap = true;
    control.trap_cause = cause;
    return control;
}

RV32IControlSignals regAlu(uint8_t alu_op,
                           RV32IALUSourceA alu_a,
                           RV32IALUSourceB alu_b,
                           bool uses_rs1,
                           bool uses_rs2,
                           bool uses_immediate) {
    auto control = baseLegal(alu_op, alu_a, alu_b);
    control.reg_write = true;
    control.writeback = RV32IWritebackSource::ALU;
    control.uses_rs1 = uses_rs1;
    control.uses_rs2 = uses_rs2;
    control.uses_rd = true;
    control.uses_immediate = uses_immediate;
    return control;
}

RV32IControlSignals loadControl(RV32IMemorySize size, bool sign_extend) {
    auto control = baseLegal(ALU32Op::ADD, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate);
    control.reg_write = true;
    control.writeback = RV32IWritebackSource::Memory;
    control.mem_read = true;
    control.mem_size = size;
    control.load_sign_extend = sign_extend;
    control.uses_rs1 = true;
    control.uses_rd = true;
    control.uses_immediate = true;
    return control;
}

RV32IControlSignals storeControl(RV32IMemorySize size) {
    auto control = baseLegal(ALU32Op::ADD, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate);
    control.mem_write = true;
    control.mem_size = size;
    control.uses_rs1 = true;
    control.uses_rs2 = true;
    control.uses_immediate = true;
    return control;
}

RV32IControlSignals branchControl(RV32IBranchType branch) {
    auto control = baseLegal(ALU32Op::SUB, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2);
    control.branch = branch;
    control.uses_rs1 = true;
    control.uses_rs2 = true;
    control.uses_immediate = true;
    return control;
}

RV32IControlSignals jumpControl(RV32IJumpType jump,
                                RV32IALUSourceA alu_a,
                                RV32IALUSourceB alu_b,
                                bool uses_rs1) {
    auto control = baseLegal(ALU32Op::ADD, alu_a, alu_b);
    control.reg_write = true;
    control.writeback = RV32IWritebackSource::PCPlus4;
    control.jump = jump;
    control.uses_rs1 = uses_rs1;
    control.uses_rd = true;
    control.uses_immediate = true;
    return control;
}
}

RV32IControlSignals RV32IControl::fromDecoded(const RV32IDecodedInstruction& decoded) {
    if (!decoded.legal) {
        return trapControl(RV32ITrapCause::IllegalInstruction);
    }

    switch (decoded.instruction) {
        case RV32IInstruction::ADD: return regAlu(ALU32Op::ADD, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::SUB: return regAlu(ALU32Op::SUB, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::SLL: return regAlu(ALU32Op::SLL, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::SLT: return regAlu(ALU32Op::SLT, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::SLTU: return regAlu(ALU32Op::SLTU, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::XOR: return regAlu(ALU32Op::XOR, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::SRL: return regAlu(ALU32Op::SRL, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::SRA: return regAlu(ALU32Op::SRA, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::OR: return regAlu(ALU32Op::OR, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);
        case RV32IInstruction::AND: return regAlu(ALU32Op::AND, RV32IALUSourceA::RS1, RV32IALUSourceB::RS2, true, true, false);

        case RV32IInstruction::ADDI: return regAlu(ALU32Op::ADD, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::SLTI: return regAlu(ALU32Op::SLT, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::SLTIU: return regAlu(ALU32Op::SLTU, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::XORI: return regAlu(ALU32Op::XOR, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::ORI: return regAlu(ALU32Op::OR, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::ANDI: return regAlu(ALU32Op::AND, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::SLLI: return regAlu(ALU32Op::SLL, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::SRLI: return regAlu(ALU32Op::SRL, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);
        case RV32IInstruction::SRAI: return regAlu(ALU32Op::SRA, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true, false, true);

        case RV32IInstruction::LB: return loadControl(RV32IMemorySize::Byte, true);
        case RV32IInstruction::LH: return loadControl(RV32IMemorySize::Halfword, true);
        case RV32IInstruction::LW: return loadControl(RV32IMemorySize::Word, false);
        case RV32IInstruction::LBU: return loadControl(RV32IMemorySize::Byte, false);
        case RV32IInstruction::LHU: return loadControl(RV32IMemorySize::Halfword, false);

        case RV32IInstruction::SB: return storeControl(RV32IMemorySize::Byte);
        case RV32IInstruction::SH: return storeControl(RV32IMemorySize::Halfword);
        case RV32IInstruction::SW: return storeControl(RV32IMemorySize::Word);

        case RV32IInstruction::BEQ: return branchControl(RV32IBranchType::BEQ);
        case RV32IInstruction::BNE: return branchControl(RV32IBranchType::BNE);
        case RV32IInstruction::BLT: return branchControl(RV32IBranchType::BLT);
        case RV32IInstruction::BGE: return branchControl(RV32IBranchType::BGE);
        case RV32IInstruction::BLTU: return branchControl(RV32IBranchType::BLTU);
        case RV32IInstruction::BGEU: return branchControl(RV32IBranchType::BGEU);

        case RV32IInstruction::JAL: return jumpControl(RV32IJumpType::JAL, RV32IALUSourceA::PC, RV32IALUSourceB::Immediate, false);
        case RV32IInstruction::JALR: return jumpControl(RV32IJumpType::JALR, RV32IALUSourceA::RS1, RV32IALUSourceB::Immediate, true);
        case RV32IInstruction::LUI: return regAlu(ALU32Op::PASS_B, RV32IALUSourceA::Zero, RV32IALUSourceB::Immediate, false, false, true);
        case RV32IInstruction::AUIPC: return regAlu(ALU32Op::ADD, RV32IALUSourceA::PC, RV32IALUSourceB::Immediate, false, false, true);

        case RV32IInstruction::FENCE:
            return baseLegal(ALU32Op::ZERO, RV32IALUSourceA::Zero, RV32IALUSourceB::Zero);

        case RV32IInstruction::ECALL: {
            auto control = trapControl(RV32ITrapCause::EnvironmentCall);
            control.legal = true;
            return control;
        }

        case RV32IInstruction::EBREAK: {
            auto control = baseLegal(ALU32Op::ZERO, RV32IALUSourceA::Zero, RV32IALUSourceB::Zero);
            control.halt = true;
            return control;
        }

        case RV32IInstruction::INVALID:
            return trapControl(RV32ITrapCause::IllegalInstruction);
    }

    return trapControl(RV32ITrapCause::IllegalInstruction);
}

RV32IControlSignals RV32IControl::fromRaw(uint32_t raw) {
    return fromDecoded(RV32IDecoder::decode(raw));
}

std::string_view toString(RV32IALUSourceA source) {
    switch (source) {
        case RV32IALUSourceA::RS1: return "RS1";
        case RV32IALUSourceA::PC: return "PC";
        case RV32IALUSourceA::Zero: return "Zero";
    }
    return "Zero";
}

std::string_view toString(RV32IALUSourceB source) {
    switch (source) {
        case RV32IALUSourceB::RS2: return "RS2";
        case RV32IALUSourceB::Immediate: return "Immediate";
        case RV32IALUSourceB::Zero: return "Zero";
    }
    return "Zero";
}

std::string_view toString(RV32IWritebackSource source) {
    switch (source) {
        case RV32IWritebackSource::None: return "None";
        case RV32IWritebackSource::ALU: return "ALU";
        case RV32IWritebackSource::Memory: return "Memory";
        case RV32IWritebackSource::PCPlus4: return "PCPlus4";
    }
    return "None";
}

std::string_view toString(RV32IMemorySize size) {
    switch (size) {
        case RV32IMemorySize::None: return "None";
        case RV32IMemorySize::Byte: return "Byte";
        case RV32IMemorySize::Halfword: return "Halfword";
        case RV32IMemorySize::Word: return "Word";
    }
    return "None";
}

std::string_view toString(RV32IBranchType branch) {
    switch (branch) {
        case RV32IBranchType::None: return "None";
        case RV32IBranchType::BEQ: return "BEQ";
        case RV32IBranchType::BNE: return "BNE";
        case RV32IBranchType::BLT: return "BLT";
        case RV32IBranchType::BGE: return "BGE";
        case RV32IBranchType::BLTU: return "BLTU";
        case RV32IBranchType::BGEU: return "BGEU";
    }
    return "None";
}

std::string_view toString(RV32IJumpType jump) {
    switch (jump) {
        case RV32IJumpType::None: return "None";
        case RV32IJumpType::JAL: return "JAL";
        case RV32IJumpType::JALR: return "JALR";
    }
    return "None";
}

std::string_view toString(RV32ITrapCause cause) {
    switch (cause) {
        case RV32ITrapCause::None: return "None";
        case RV32ITrapCause::IllegalInstruction: return "IllegalInstruction";
        case RV32ITrapCause::EnvironmentCall: return "EnvironmentCall";
    }
    return "None";
}

}
