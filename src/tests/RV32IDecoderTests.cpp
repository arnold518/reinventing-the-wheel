#include "tests/RV32IDecoderTests.hpp"

#include "rv32i/RV32IDecoder.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using rv32i::RV32IDecodedInstruction;
using rv32i::RV32IDecodeStatus;
using rv32i::RV32IDecoder;
using rv32i::RV32IFormat;
using rv32i::RV32IInstruction;

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

void fail(const std::string& message) {
    std::cerr << "RV32IDecoderTest failed: " << message << std::endl;
    assert(false && "RV32I decoder test failed");
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

std::string describeRaw(uint32_t raw) {
    std::ostringstream out;
    out << "0x" << std::hex << raw;
    return out.str();
}

void expectDecoded(uint32_t raw,
                   RV32IInstruction instruction,
                   RV32IFormat format,
                   uint8_t rd,
                   uint8_t rs1,
                   uint8_t rs2,
                   int32_t immediate,
                   const std::string& label) {
    const auto decoded = RV32IDecoder::decode(raw);
    expect(decoded.legal, label + " should be legal");
    expect(decoded.status == RV32IDecodeStatus::Legal, label + " status");
    expect(decoded.instruction == instruction, label + " instruction");
    expect(decoded.format == format, label + " format");
    if (rd != 0xFF) expect(decoded.rd == rd, label + " rd");
    if (rs1 != 0xFF) expect(decoded.rs1 == rs1, label + " rs1");
    if (rs2 != 0xFF) expect(decoded.rs2 == rs2, label + " rs2");
    expect(decoded.immediate == immediate, label + " immediate");
}

void expectInvalid(uint32_t raw, RV32IDecodeStatus status, const std::string& label) {
    const auto decoded = RV32IDecoder::decode(raw);
    expect(!decoded.legal, label + " should be illegal");
    expect(decoded.instruction == RV32IInstruction::INVALID, label + " invalid instruction");
    expect(decoded.format == RV32IFormat::Invalid, label + " invalid format");
    expect(decoded.status == status, label + " status");
}

void testFieldExtraction() {
    const uint32_t raw = encodeR(0x20, 12, 11, 0x0, 10);
    expect(RV32IDecoder::opcode(raw) == 0x33, "opcode extraction");
    expect(RV32IDecoder::rd(raw) == 10, "rd extraction");
    expect(RV32IDecoder::funct3(raw) == 0, "funct3 extraction");
    expect(RV32IDecoder::rs1(raw) == 11, "rs1 extraction");
    expect(RV32IDecoder::rs2(raw) == 12, "rs2 extraction");
    expect(RV32IDecoder::funct7(raw) == 0x20, "funct7 extraction");

    const uint32_t i_raw = encodeI(-1, 5, 0, 6, 0x13);
    expect(RV32IDecoder::imm12(i_raw) == 0xFFF, "imm12 extraction");
    expect(RV32IDecoder::shamt(encodeShiftI(0, 31, 1, 1, 2)) == 31, "shamt extraction");
}

void testSignExtensionAndImmediateHelpers() {
    expect(RV32IDecoder::signExtend(0x7FF, 12) == 2047, "sign extend positive 12-bit");
    expect(RV32IDecoder::signExtend(0x800, 12) == -2048, "sign extend negative 12-bit");
    expect(RV32IDecoder::immediateI(encodeI(-1, 1, 0, 2, 0x13)) == -1, "I immediate -1");
    expect(RV32IDecoder::immediateI(encodeI(2047, 1, 0, 2, 0x13)) == 2047, "I immediate max");
    expect(RV32IDecoder::immediateS(encodeS(-2048, 2, 1, 2)) == -2048, "S immediate min");
    expect(RV32IDecoder::immediateS(encodeS(2047, 2, 1, 2)) == 2047, "S immediate max");
    expect(RV32IDecoder::immediateB(encodeB(-4096, 2, 1, 0)) == -4096, "B immediate min");
    expect(RV32IDecoder::immediateB(encodeB(4094, 2, 1, 0)) == 4094, "B immediate max");
    expect(RV32IDecoder::immediateU(encodeU(0xFFFFF000U, 3, 0x37)) == static_cast<int32_t>(0xFFFFF000U), "U immediate sign bit kept");
    expect(RV32IDecoder::immediateJ(encodeJ(-1048576, 1)) == -1048576, "J immediate min");
    expect(RV32IDecoder::immediateJ(encodeJ(1048574, 1)) == 1048574, "J immediate max");
}

void testValidInstructions() {
    struct Case {
        uint32_t raw;
        RV32IInstruction instruction;
        RV32IFormat format;
        uint8_t rd;
        uint8_t rs1;
        uint8_t rs2;
        int32_t immediate;
        const char* label;
    };

    const std::vector<Case> cases{
        {encodeR(0x00, 12, 11, 0x0, 10), RV32IInstruction::ADD, RV32IFormat::R, 10, 11, 12, 0, "ADD"},
        {encodeR(0x20, 12, 11, 0x0, 10), RV32IInstruction::SUB, RV32IFormat::R, 10, 11, 12, 0, "SUB"},
        {encodeR(0x00, 12, 11, 0x1, 10), RV32IInstruction::SLL, RV32IFormat::R, 10, 11, 12, 0, "SLL"},
        {encodeR(0x00, 12, 11, 0x2, 10), RV32IInstruction::SLT, RV32IFormat::R, 10, 11, 12, 0, "SLT"},
        {encodeR(0x00, 12, 11, 0x3, 10), RV32IInstruction::SLTU, RV32IFormat::R, 10, 11, 12, 0, "SLTU"},
        {encodeR(0x00, 12, 11, 0x4, 10), RV32IInstruction::XOR, RV32IFormat::R, 10, 11, 12, 0, "XOR"},
        {encodeR(0x00, 12, 11, 0x5, 10), RV32IInstruction::SRL, RV32IFormat::R, 10, 11, 12, 0, "SRL"},
        {encodeR(0x20, 12, 11, 0x5, 10), RV32IInstruction::SRA, RV32IFormat::R, 10, 11, 12, 0, "SRA"},
        {encodeR(0x00, 12, 11, 0x6, 10), RV32IInstruction::OR, RV32IFormat::R, 10, 11, 12, 0, "OR"},
        {encodeR(0x00, 12, 11, 0x7, 10), RV32IInstruction::AND, RV32IFormat::R, 10, 11, 12, 0, "AND"},

        {encodeI(-1, 5, 0x0, 6, 0x13), RV32IInstruction::ADDI, RV32IFormat::I, 6, 5, 31, -1, "ADDI"},
        {encodeI(-2048, 5, 0x2, 6, 0x13), RV32IInstruction::SLTI, RV32IFormat::I, 6, 5, 0, -2048, "SLTI"},
        {encodeI(2047, 5, 0x3, 6, 0x13), RV32IInstruction::SLTIU, RV32IFormat::I, 6, 5, 31, 2047, "SLTIU"},
        {encodeI(0x55, 5, 0x4, 6, 0x13), RV32IInstruction::XORI, RV32IFormat::I, 6, 5, 21, 0x55, "XORI"},
        {encodeI(0x123, 5, 0x6, 6, 0x13), RV32IInstruction::ORI, RV32IFormat::I, 6, 5, 3, 0x123, "ORI"},
        {encodeI(0x321, 5, 0x7, 6, 0x13), RV32IInstruction::ANDI, RV32IFormat::I, 6, 5, 1, 0x321, "ANDI"},
        {encodeShiftI(0x00, 5, 5, 0x1, 6), RV32IInstruction::SLLI, RV32IFormat::I, 6, 5, 5, 5, "SLLI"},
        {encodeShiftI(0x00, 6, 5, 0x5, 6), RV32IInstruction::SRLI, RV32IFormat::I, 6, 5, 6, 6, "SRLI"},
        {encodeShiftI(0x20, 31, 5, 0x5, 6), RV32IInstruction::SRAI, RV32IFormat::I, 6, 5, 31, 1055, "SRAI"},

        {encodeI(-16, 3, 0x0, 4, 0x03), RV32IInstruction::LB, RV32IFormat::I, 4, 3, 16, -16, "LB"},
        {encodeI(18, 3, 0x1, 4, 0x03), RV32IInstruction::LH, RV32IFormat::I, 4, 3, 18, 18, "LH"},
        {encodeI(20, 3, 0x2, 4, 0x03), RV32IInstruction::LW, RV32IFormat::I, 4, 3, 20, 20, "LW"},
        {encodeI(21, 3, 0x4, 4, 0x03), RV32IInstruction::LBU, RV32IFormat::I, 4, 3, 21, 21, "LBU"},
        {encodeI(22, 3, 0x5, 4, 0x03), RV32IInstruction::LHU, RV32IFormat::I, 4, 3, 22, 22, "LHU"},

        {encodeS(-20, 8, 7, 0x0), RV32IInstruction::SB, RV32IFormat::S, 0xFF, 7, 8, -20, "SB"},
        {encodeS(24, 8, 7, 0x1), RV32IInstruction::SH, RV32IFormat::S, 0xFF, 7, 8, 24, "SH"},
        {encodeS(28, 8, 7, 0x2), RV32IInstruction::SW, RV32IFormat::S, 0xFF, 7, 8, 28, "SW"},

        {encodeB(16, 2, 1, 0x0), RV32IInstruction::BEQ, RV32IFormat::B, 0xFF, 1, 2, 16, "BEQ"},
        {encodeB(-4, 2, 1, 0x1), RV32IInstruction::BNE, RV32IFormat::B, 0xFF, 1, 2, -4, "BNE"},
        {encodeB(32, 2, 1, 0x4), RV32IInstruction::BLT, RV32IFormat::B, 0xFF, 1, 2, 32, "BLT"},
        {encodeB(-32, 2, 1, 0x5), RV32IInstruction::BGE, RV32IFormat::B, 0xFF, 1, 2, -32, "BGE"},
        {encodeB(64, 2, 1, 0x6), RV32IInstruction::BLTU, RV32IFormat::B, 0xFF, 1, 2, 64, "BLTU"},
        {encodeB(-64, 2, 1, 0x7), RV32IInstruction::BGEU, RV32IFormat::B, 0xFF, 1, 2, -64, "BGEU"},

        {encodeJ(2048, 1), RV32IInstruction::JAL, RV32IFormat::J, 1, 0xFF, 0xFF, 2048, "JAL"},
        {encodeI(-12, 5, 0x0, 1, 0x67), RV32IInstruction::JALR, RV32IFormat::I, 1, 5, 20, -12, "JALR"},
        {encodeU(0x12345000, 9, 0x37), RV32IInstruction::LUI, RV32IFormat::U, 9, 0xFF, 0xFF, 0x12345000, "LUI"},
        {encodeU(0xFFFFF000, 9, 0x17), RV32IInstruction::AUIPC, RV32IFormat::U, 9, 0xFF, 0xFF, static_cast<int32_t>(0xFFFFF000U), "AUIPC"},
        {encodeI(0x0FF, 0, 0x0, 0, 0x0F), RV32IInstruction::FENCE, RV32IFormat::I, 0xFF, 0xFF, 0xFF, 0x0FF, "FENCE"},
        {0x00000073U, RV32IInstruction::ECALL, RV32IFormat::I, 0xFF, 0xFF, 0xFF, 0, "ECALL"},
        {0x00100073U, RV32IInstruction::EBREAK, RV32IFormat::I, 0xFF, 0xFF, 0xFF, 1, "EBREAK"},
    };

    for (const auto& test_case : cases) {
        expectDecoded(test_case.raw,
                      test_case.instruction,
                      test_case.format,
                      test_case.rd,
                      test_case.rs1,
                      test_case.rs2,
                      test_case.immediate,
                      std::string(test_case.label) + " " + describeRaw(test_case.raw));

        const auto decoded = RV32IDecoder::decode(test_case.raw);
        if (test_case.instruction == RV32IInstruction::SLLI ||
            test_case.instruction == RV32IInstruction::SRLI ||
            test_case.instruction == RV32IInstruction::SRAI) {
            expect(decoded.shamt == test_case.rs2, std::string(test_case.label) + " shamt");
        }
    }
}

void testInvalidInstructions() {
    expectInvalid(0x00000000U, RV32IDecodeStatus::InvalidInstructionLength, "low bits not 11");
    expectInvalid(0x0000007BU, RV32IDecodeStatus::UnknownOpcode, "unknown opcode");
    expectInvalid(encodeI(0, 1, 0, 2, 0x1B), RV32IDecodeStatus::UnsupportedExtension, "RV64 OP-IMM-32");
    expectInvalid(encodeR(0, 2, 1, 0, 3, 0x3B), RV32IDecodeStatus::UnsupportedExtension, "RV64 OP-32");
    expectInvalid(encodeR(0x01, 2, 1, 0x0, 3), RV32IDecodeStatus::UnsupportedFunct7, "bad ADD funct7");
    expectInvalid(encodeR(0x01, 2, 1, 0x5, 3), RV32IDecodeStatus::UnsupportedFunct7, "bad SRL/SRA funct7");
    expectInvalid(encodeShiftI(0x01, 5, 1, 0x1, 2), RV32IDecodeStatus::UnsupportedFunct7, "bad SLLI funct7");
    expectInvalid(encodeShiftI(0x01, 5, 1, 0x5, 2), RV32IDecodeStatus::UnsupportedFunct7, "bad SRLI/SRAI funct7");
    expectInvalid(encodeB(4, 2, 1, 0x2), RV32IDecodeStatus::UnsupportedFunct3, "bad branch funct3");
    expectInvalid(encodeI(0, 1, 0x3, 2, 0x03), RV32IDecodeStatus::UnsupportedFunct3, "bad load funct3");
    expectInvalid(encodeS(0, 2, 1, 0x3), RV32IDecodeStatus::UnsupportedFunct3, "bad store funct3");
    expectInvalid(encodeI(0, 1, 0x1, 2, 0x67), RV32IDecodeStatus::UnsupportedFunct3, "bad JALR funct3");
    expectInvalid(encodeI(0, 0, 0x1, 0, 0x0F), RV32IDecodeStatus::UnsupportedExtension, "FENCE.I is not RV32I base");
    expectInvalid(0x00101073U, RV32IDecodeStatus::UnsupportedExtension, "CSR instruction is outside RV32I base");
    expectInvalid(0x00200073U, RV32IDecodeStatus::UnsupportedSystem, "unsupported SYSTEM funct12");
}
}

std::string RV32IDecoderTest::getTestName() const {
    return "RV32IDecoderTest";
}

void RV32IDecoderTest::verifyResults() {
    testFieldExtraction();
    testSignExtensionAndImmediateHelpers();
    testValidInstructions();
    testInvalidInstructions();
}
