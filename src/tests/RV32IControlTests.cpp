#include "tests/RV32IControlTests.hpp"

#include "modules/composite/ALU32.hpp"
#include "rv32i/RV32IControl.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using rv32i::RV32IALUSourceA;
using rv32i::RV32IALUSourceB;
using rv32i::RV32IBranchType;
using rv32i::RV32IControl;
using rv32i::RV32IControlSignals;
using rv32i::RV32IJumpType;
using rv32i::RV32IMemorySize;
using rv32i::RV32ITrapCause;
using rv32i::RV32IWritebackSource;

uint32_t maskSigned(int32_t value, unsigned bits) {
    return static_cast<uint32_t>(value) & ((uint32_t{1} << bits) - 1);
}

uint32_t encodeR(uint8_t funct7, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode = 0x33) {
    return (static_cast<uint32_t>(funct7) << 25)
         | (static_cast<uint32_t>(rs2) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | (static_cast<uint32_t>(rd) << 7)
         | opcode;
}

uint32_t encodeI(int32_t imm, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
    return (maskSigned(imm, 12) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | (static_cast<uint32_t>(rd) << 7)
         | opcode;
}

uint32_t encodeShiftI(uint8_t funct7, uint8_t shamt, uint8_t rs1, uint8_t funct3, uint8_t rd) {
    return (static_cast<uint32_t>(funct7) << 25)
         | (static_cast<uint32_t>(shamt & 0x1F) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | (static_cast<uint32_t>(rd) << 7)
         | 0x13U;
}

uint32_t encodeS(int32_t imm, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t encoded = maskSigned(imm, 12);
    return (((encoded >> 5) & 0x7F) << 25)
         | (static_cast<uint32_t>(rs2) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | ((encoded & 0x1F) << 7)
         | 0x23U;
}

uint32_t encodeB(int32_t imm, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t encoded = maskSigned(imm, 13);
    return (((encoded >> 12) & 0x1) << 31)
         | (((encoded >> 5) & 0x3F) << 25)
         | (static_cast<uint32_t>(rs2) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | (((encoded >> 1) & 0xF) << 8)
         | (((encoded >> 11) & 0x1) << 7)
         | 0x63U;
}

uint32_t encodeU(uint32_t imm, uint8_t rd, uint8_t opcode) {
    return (imm & 0xFFFFF000U)
         | (static_cast<uint32_t>(rd) << 7)
         | opcode;
}

uint32_t encodeJ(int32_t imm, uint8_t rd) {
    const uint32_t encoded = maskSigned(imm, 21);
    return (((encoded >> 20) & 0x1) << 31)
         | (((encoded >> 1) & 0x3FF) << 21)
         | (((encoded >> 11) & 0x1) << 20)
         | (((encoded >> 12) & 0xFF) << 12)
         | (static_cast<uint32_t>(rd) << 7)
         | 0x6FU;
}

struct Expected {
    uint32_t raw = 0;
    uint8_t alu_op = ALU32Op::ZERO;
    RV32IALUSourceA alu_a = RV32IALUSourceA::Zero;
    RV32IALUSourceB alu_b = RV32IALUSourceB::Zero;
    bool reg_write = false;
    RV32IWritebackSource writeback = RV32IWritebackSource::None;
    bool mem_read = false;
    bool mem_write = false;
    RV32IMemorySize mem_size = RV32IMemorySize::None;
    bool load_sign_extend = false;
    RV32IBranchType branch = RV32IBranchType::None;
    RV32IJumpType jump = RV32IJumpType::None;
    bool uses_rs1 = false;
    bool uses_rs2 = false;
    bool uses_rd = false;
    bool uses_immediate = false;
    bool legal = true;
    bool halt = false;
    bool trap = false;
    RV32ITrapCause trap_cause = RV32ITrapCause::None;
    const char* label = "";
};

void fail(const std::string& label, const std::string& field) {
    throw std::runtime_error("RV32IControlTest failed for " + label + ": " + field);
}

template <typename T>
void expectEq(const T& actual, const T& expected, const std::string& label, const std::string& field) {
    if (actual != expected) {
        fail(label, field);
    }
}

void expectControl(const Expected& expected) {
    const RV32IControlSignals actual = RV32IControl::fromRaw(expected.raw);
    const std::string label = expected.label;
    expectEq(actual.legal, expected.legal, label, "legal");
    expectEq(actual.alu_op, expected.alu_op, label, "alu_op");
    expectEq(actual.alu_a, expected.alu_a, label, "alu_a");
    expectEq(actual.alu_b, expected.alu_b, label, "alu_b");
    expectEq(actual.reg_write, expected.reg_write, label, "reg_write");
    expectEq(actual.writeback, expected.writeback, label, "writeback");
    expectEq(actual.mem_read, expected.mem_read, label, "mem_read");
    expectEq(actual.mem_write, expected.mem_write, label, "mem_write");
    expectEq(actual.mem_size, expected.mem_size, label, "mem_size");
    expectEq(actual.load_sign_extend, expected.load_sign_extend, label, "load_sign_extend");
    expectEq(actual.branch, expected.branch, label, "branch");
    expectEq(actual.jump, expected.jump, label, "jump");
    expectEq(actual.uses_rs1, expected.uses_rs1, label, "uses_rs1");
    expectEq(actual.uses_rs2, expected.uses_rs2, label, "uses_rs2");
    expectEq(actual.uses_rd, expected.uses_rd, label, "uses_rd");
    expectEq(actual.uses_immediate, expected.uses_immediate, label, "uses_immediate");
    expectEq(actual.halt, expected.halt, label, "halt");
    expectEq(actual.trap, expected.trap, label, "trap");
    expectEq(actual.trap_cause, expected.trap_cause, label, "trap_cause");
}

Expected rAlu(const char* label, uint32_t raw, uint8_t alu_op) {
    Expected expected;
    expected.label = label;
    expected.raw = raw;
    expected.alu_op = alu_op;
    expected.alu_a = RV32IALUSourceA::RS1;
    expected.alu_b = RV32IALUSourceB::RS2;
    expected.reg_write = true;
    expected.writeback = RV32IWritebackSource::ALU;
    expected.uses_rs1 = true;
    expected.uses_rs2 = true;
    expected.uses_rd = true;
    return expected;
}

Expected iAlu(const char* label, uint32_t raw, uint8_t alu_op) {
    Expected expected = rAlu(label, raw, alu_op);
    expected.alu_b = RV32IALUSourceB::Immediate;
    expected.uses_rs2 = false;
    expected.uses_immediate = true;
    return expected;
}

Expected load(const char* label, uint32_t raw, RV32IMemorySize size, bool sign_extend) {
    Expected expected = iAlu(label, raw, ALU32Op::ADD);
    expected.writeback = RV32IWritebackSource::Memory;
    expected.mem_read = true;
    expected.mem_size = size;
    expected.load_sign_extend = sign_extend;
    return expected;
}

Expected store(const char* label, uint32_t raw, RV32IMemorySize size) {
    Expected expected;
    expected.label = label;
    expected.raw = raw;
    expected.alu_op = ALU32Op::ADD;
    expected.alu_a = RV32IALUSourceA::RS1;
    expected.alu_b = RV32IALUSourceB::Immediate;
    expected.mem_write = true;
    expected.mem_size = size;
    expected.uses_rs1 = true;
    expected.uses_rs2 = true;
    expected.uses_immediate = true;
    return expected;
}

Expected branch(const char* label, uint32_t raw, RV32IBranchType branch_type) {
    Expected expected;
    expected.label = label;
    expected.raw = raw;
    expected.alu_op = ALU32Op::SUB;
    expected.alu_a = RV32IALUSourceA::RS1;
    expected.alu_b = RV32IALUSourceB::RS2;
    expected.branch = branch_type;
    expected.uses_rs1 = true;
    expected.uses_rs2 = true;
    expected.uses_immediate = true;
    return expected;
}

void testControlMappings() {
    const std::vector<Expected> cases{
        rAlu("ADD", encodeR(0x00, 12, 11, 0x0, 10), ALU32Op::ADD),
        rAlu("SUB", encodeR(0x20, 12, 11, 0x0, 10), ALU32Op::SUB),
        rAlu("SLL", encodeR(0x00, 12, 11, 0x1, 10), ALU32Op::SLL),
        rAlu("SLT", encodeR(0x00, 12, 11, 0x2, 10), ALU32Op::SLT),
        rAlu("SLTU", encodeR(0x00, 12, 11, 0x3, 10), ALU32Op::SLTU),
        rAlu("XOR", encodeR(0x00, 12, 11, 0x4, 10), ALU32Op::XOR),
        rAlu("SRL", encodeR(0x00, 12, 11, 0x5, 10), ALU32Op::SRL),
        rAlu("SRA", encodeR(0x20, 12, 11, 0x5, 10), ALU32Op::SRA),
        rAlu("OR", encodeR(0x00, 12, 11, 0x6, 10), ALU32Op::OR),
        rAlu("AND", encodeR(0x00, 12, 11, 0x7, 10), ALU32Op::AND),

        iAlu("ADDI", encodeI(-1, 5, 0x0, 6, 0x13), ALU32Op::ADD),
        iAlu("SLTI", encodeI(-2048, 5, 0x2, 6, 0x13), ALU32Op::SLT),
        iAlu("SLTIU", encodeI(2047, 5, 0x3, 6, 0x13), ALU32Op::SLTU),
        iAlu("XORI", encodeI(0x55, 5, 0x4, 6, 0x13), ALU32Op::XOR),
        iAlu("ORI", encodeI(0x123, 5, 0x6, 6, 0x13), ALU32Op::OR),
        iAlu("ANDI", encodeI(0x321, 5, 0x7, 6, 0x13), ALU32Op::AND),
        iAlu("SLLI", encodeShiftI(0x00, 5, 5, 0x1, 6), ALU32Op::SLL),
        iAlu("SRLI", encodeShiftI(0x00, 6, 5, 0x5, 6), ALU32Op::SRL),
        iAlu("SRAI", encodeShiftI(0x20, 31, 5, 0x5, 6), ALU32Op::SRA),

        load("LB", encodeI(-16, 3, 0x0, 4, 0x03), RV32IMemorySize::Byte, true),
        load("LH", encodeI(18, 3, 0x1, 4, 0x03), RV32IMemorySize::Halfword, true),
        load("LW", encodeI(20, 3, 0x2, 4, 0x03), RV32IMemorySize::Word, false),
        load("LBU", encodeI(21, 3, 0x4, 4, 0x03), RV32IMemorySize::Byte, false),
        load("LHU", encodeI(22, 3, 0x5, 4, 0x03), RV32IMemorySize::Halfword, false),

        store("SB", encodeS(-20, 8, 7, 0x0), RV32IMemorySize::Byte),
        store("SH", encodeS(24, 8, 7, 0x1), RV32IMemorySize::Halfword),
        store("SW", encodeS(28, 8, 7, 0x2), RV32IMemorySize::Word),

        branch("BEQ", encodeB(16, 2, 1, 0x0), RV32IBranchType::BEQ),
        branch("BNE", encodeB(-4, 2, 1, 0x1), RV32IBranchType::BNE),
        branch("BLT", encodeB(32, 2, 1, 0x4), RV32IBranchType::BLT),
        branch("BGE", encodeB(-32, 2, 1, 0x5), RV32IBranchType::BGE),
        branch("BLTU", encodeB(64, 2, 1, 0x6), RV32IBranchType::BLTU),
        branch("BGEU", encodeB(-64, 2, 1, 0x7), RV32IBranchType::BGEU),

        [] {
            Expected expected;
            expected.label = "JAL";
            expected.raw = encodeJ(2048, 1);
            expected.alu_op = ALU32Op::ADD;
            expected.alu_a = RV32IALUSourceA::PC;
            expected.alu_b = RV32IALUSourceB::Immediate;
            expected.reg_write = true;
            expected.writeback = RV32IWritebackSource::PCPlus4;
            expected.jump = RV32IJumpType::JAL;
            expected.uses_rd = true;
            expected.uses_immediate = true;
            return expected;
        }(),
        [] {
            Expected expected;
            expected.label = "JALR";
            expected.raw = encodeI(-12, 5, 0x0, 1, 0x67);
            expected.alu_op = ALU32Op::ADD;
            expected.alu_a = RV32IALUSourceA::RS1;
            expected.alu_b = RV32IALUSourceB::Immediate;
            expected.reg_write = true;
            expected.writeback = RV32IWritebackSource::PCPlus4;
            expected.jump = RV32IJumpType::JALR;
            expected.uses_rs1 = true;
            expected.uses_rd = true;
            expected.uses_immediate = true;
            return expected;
        }(),
        [] {
            Expected expected;
            expected.label = "LUI";
            expected.raw = encodeU(0x12345000, 9, 0x37);
            expected.alu_op = ALU32Op::PASS_B;
            expected.alu_a = RV32IALUSourceA::Zero;
            expected.alu_b = RV32IALUSourceB::Immediate;
            expected.reg_write = true;
            expected.writeback = RV32IWritebackSource::ALU;
            expected.uses_rd = true;
            expected.uses_immediate = true;
            return expected;
        }(),
        [] {
            Expected expected;
            expected.label = "AUIPC";
            expected.raw = encodeU(0xFFFFF000, 9, 0x17);
            expected.alu_op = ALU32Op::ADD;
            expected.alu_a = RV32IALUSourceA::PC;
            expected.alu_b = RV32IALUSourceB::Immediate;
            expected.reg_write = true;
            expected.writeback = RV32IWritebackSource::ALU;
            expected.uses_rd = true;
            expected.uses_immediate = true;
            return expected;
        }(),
        [] {
            Expected expected;
            expected.label = "FENCE";
            expected.raw = encodeI(0x0FF, 0, 0x0, 0, 0x0F);
            expected.alu_op = ALU32Op::ZERO;
            return expected;
        }(),
        [] {
            Expected expected;
            expected.label = "ECALL";
            expected.raw = 0x00000073U;
            expected.trap = true;
            expected.trap_cause = RV32ITrapCause::EnvironmentCall;
            return expected;
        }(),
        [] {
            Expected expected;
            expected.label = "EBREAK";
            expected.raw = 0x00100073U;
            expected.alu_op = ALU32Op::ZERO;
            expected.halt = true;
            return expected;
        }(),
        [] {
            Expected expected;
            expected.label = "INVALID";
            expected.raw = 0x00000000U;
            expected.legal = false;
            expected.trap = true;
            expected.trap_cause = RV32ITrapCause::IllegalInstruction;
            return expected;
        }(),
    };

    for (const auto& test_case : cases) {
        expectControl(test_case);
    }
}
}

std::string RV32IControlTest::getTestName() const {
    return "RV32IControlTest";
}

void RV32IControlTest::verifyResults() {
    testControlMappings();
}
