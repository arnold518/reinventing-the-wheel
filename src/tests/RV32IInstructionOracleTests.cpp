#include "tests/RV32IInstructionOracleTests.hpp"

#include "rv32i/RV32IInstructionOracle.hpp"
#include "rv32i/RV32IProgram.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using rv32i::RV32IExecutionTrapCause;
using rv32i::RV32IInstructionOracle;
using rv32i::RV32IFunctionalMemory;
using rv32i::RV32IMemoryAccessKind;
using rv32i::RV32IMemorySize;
using rv32i::RV32IProgram;
using rv32i::RV32IState;
using rv32i::RV32IInstructionTrace;

uint32_t maskSigned(int32_t value, unsigned bits) {
    return static_cast<uint32_t>(value) & ((uint32_t{1} << bits) - 1);
}

uint32_t encodeR(uint8_t funct7, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t rd) {
    return (static_cast<uint32_t>(funct7) << 25)
         | (static_cast<uint32_t>(rs2) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | (static_cast<uint32_t>(rd) << 7)
         | 0x33U;
}

uint32_t encodeI(int32_t imm, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode = 0x13) {
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

std::string hex32(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << value;
    return out.str();
}

void fail(const std::string& message) {
    std::cerr << "RV32IInstructionOracleTest failed: " << message << std::endl;
    assert(false && "RV32I instruction oracle test failed");
}

void expect(bool condition, const std::string& label) {
    if (!condition) {
        fail(label);
    }
}

template <typename T>
void expectEq(const T& actual, const T& expected, const std::string& label) {
    if (actual != expected) {
        fail(label);
    }
}

void expectWord(uint32_t actual, uint32_t expected, const std::string& label) {
    if (actual != expected) {
        fail(label + ": expected " + hex32(expected) + ", got " + hex32(actual));
    }
}

RV32IInstructionTrace stepRaw(uint32_t raw, RV32IState& state, RV32IFunctionalMemory& memory) {
    memory.writeU32(state.pc, raw);
    return RV32IInstructionOracle::step(state, memory);
}

RV32IInstructionTrace stepRaw(uint32_t raw, RV32IState& state) {
    RV32IFunctionalMemory memory;
    return stepRaw(raw, state, memory);
}

void expectWriteback(const RV32IInstructionTrace& trace, uint8_t rd, uint32_t value, const std::string& label) {
    expect(trace.writeback.enabled, label + " writeback enabled");
    expectEq(trace.writeback.rd, rd, label + " writeback rd");
    expectWord(trace.writeback.value, value, label + " writeback value");
}

void testSimpleProgramTrace() {
    RV32IState state;
    RV32IFunctionalMemory memory;
    memory.loadProgram(RV32IProgram::fromWords({
        encodeI(1, 0, 0x0, 1), // addi x1, x0, 1
        encodeI(2, 1, 0x0, 2), // addi x2, x1, 2
        0x00100073U,           // ebreak
    }));

    const auto result = RV32IInstructionOracle::run(state, memory, 8);
    expectEq(result.trace.size(), static_cast<size_t>(3), "simple program trace length");
    expectWord(result.trace[0].pc_before, 0, "step 0 pc before");
    expectWriteback(result.trace[0], 1, 1, "step 0 addi");
    expectWord(result.trace[0].pc_after, 4, "step 0 pc after");
    expectEq(result.trace[1].rs1, static_cast<uint8_t>(1), "step 1 rs1 id");
    expectWord(result.trace[1].rs1_value, 1, "step 1 rs1 value");
    expectWriteback(result.trace[1], 2, 3, "step 1 addi");
    expectWord(result.trace[2].pc_before, 8, "ebreak pc");
    expect(result.trace[2].halted, "ebreak trace halted");
    expect(result.final_state.halted, "final state halted");
    expectWord(result.final_state.pc, 8, "halt keeps pc at ebreak");
    expectWord(result.final_state.readRegister(1), 1, "final x1");
    expectWord(result.final_state.readRegister(2), 3, "final x2");

    RV32IState x0_state;
    const auto x0_trace = stepRaw(encodeI(5, 0, 0x0, 0), x0_state);
    expect(x0_trace.writeback.enabled, "x0 writeback is still visible in trace");
    expect(x0_trace.writeback.ignored_x0, "x0 writeback is marked ignored");
    expectWord(x0_state.readRegister(0), 0, "x0 remains zero");
}

void testRTypeAlu() {
    struct Case {
        const char* label;
        uint8_t funct7;
        uint8_t funct3;
        uint32_t rs1;
        uint32_t rs2;
        uint32_t expected;
    };

    const std::vector<Case> cases{
        {"ADD", 0x00, 0x0, 10, 3, 13},
        {"SUB", 0x20, 0x0, 10, 3, 7},
        {"SLL", 0x00, 0x1, 1, 3, 8},
        {"SLT", 0x00, 0x2, 0xffffffffU, 1, 1},
        {"SLTU", 0x00, 0x3, 0xffffffffU, 1, 0},
        {"XOR", 0x00, 0x4, 0x0000f0f0U, 0x00000ff0U, 0x0000ff00U},
        {"SRL", 0x00, 0x5, 0x80000000U, 4, 0x08000000U},
        {"SRA", 0x20, 0x5, 0x80000000U, 4, 0xf8000000U},
        {"OR", 0x00, 0x6, 0x0000f000U, 0x000000ffU, 0x0000f0ffU},
        {"AND", 0x00, 0x7, 0x0000f0ffU, 0x00000ff0U, 0x000000f0U},
    };

    for (const auto& test_case : cases) {
        RV32IState state;
        state.writeRegister(1, test_case.rs1);
        state.writeRegister(2, test_case.rs2);
        const auto trace = stepRaw(encodeR(test_case.funct7, 2, 1, test_case.funct3, 5), state);
        expectWriteback(trace, 5, test_case.expected, test_case.label);
        expectWord(trace.alu_result, test_case.expected, std::string(test_case.label) + " ALU trace");
        expectWord(state.readRegister(5), test_case.expected, std::string(test_case.label) + " final register");
    }
}

void testITypeAlu() {
    struct Case {
        const char* label;
        uint32_t raw;
        uint32_t rs1;
        uint32_t expected;
    };

    const std::vector<Case> cases{
        {"ADDI", encodeI(-1, 1, 0x0, 5), 2, 1},
        {"SLTI", encodeI(0, 1, 0x2, 5), 0xffffffffU, 1},
        {"SLTIU", encodeI(1, 1, 0x3, 5), 0, 1},
        {"XORI", encodeI(0x0f, 1, 0x4, 5), 0xf0, 0xff},
        {"ORI", encodeI(0x0f, 1, 0x6, 5), 0xf0, 0xff},
        {"ANDI", encodeI(0x0f, 1, 0x7, 5), 0xff, 0x0f},
        {"SLLI", encodeShiftI(0x00, 4, 1, 0x1, 5), 1, 16},
        {"SRLI", encodeShiftI(0x00, 4, 1, 0x5, 5), 0x80000000U, 0x08000000U},
        {"SRAI", encodeShiftI(0x20, 4, 1, 0x5, 5), 0x80000000U, 0xf8000000U},
    };

    for (const auto& test_case : cases) {
        RV32IState state;
        state.writeRegister(1, test_case.rs1);
        const auto trace = stepRaw(test_case.raw, state);
        expectWriteback(trace, 5, test_case.expected, test_case.label);
        expectWord(state.readRegister(5), test_case.expected, std::string(test_case.label) + " final register");
    }
}

void testLoads() {
    struct Case {
        const char* label;
        uint32_t raw;
        uint32_t expected;
        RV32IMemorySize size;
        bool sign_extend;
        uint32_t address;
    };

    const std::vector<Case> cases{
        {"LB", encodeI(0, 1, 0x0, 5, 0x03), 0xffffff80U, RV32IMemorySize::Byte, true, 0x80},
        {"LBU", encodeI(0, 1, 0x4, 5, 0x03), 0x00000080U, RV32IMemorySize::Byte, false, 0x80},
        {"LH", encodeI(2, 1, 0x1, 5, 0x03), 0xffff8001U, RV32IMemorySize::Halfword, true, 0x82},
        {"LHU", encodeI(2, 1, 0x5, 5, 0x03), 0x00008001U, RV32IMemorySize::Halfword, false, 0x82},
        {"LW", encodeI(4, 1, 0x2, 5, 0x03), 0x12345678U, RV32IMemorySize::Word, false, 0x84},
    };

    for (const auto& test_case : cases) {
        RV32IState state;
        RV32IFunctionalMemory memory;
        state.writeRegister(1, 0x80);
        memory.writeU8(0x80, 0x80);
        memory.writeU16(0x82, 0x8001);
        memory.writeU32(0x84, 0x12345678U);
        const auto trace = stepRaw(test_case.raw, state, memory);
        expectEq(trace.memory.kind, RV32IMemoryAccessKind::Read, std::string(test_case.label) + " memory kind");
        expectEq(trace.memory.size, test_case.size, std::string(test_case.label) + " memory size");
        expectEq(trace.memory.sign_extend, test_case.sign_extend, std::string(test_case.label) + " sign extend");
        expectWord(trace.memory.address, test_case.address, std::string(test_case.label) + " address");
        expectWord(trace.memory.read_data, test_case.expected, std::string(test_case.label) + " read data");
        expectWriteback(trace, 5, test_case.expected, test_case.label);
        expectWord(state.readRegister(5), test_case.expected, std::string(test_case.label) + " final register");
    }
}

void testStores() {
    struct Case {
        const char* label;
        uint32_t raw;
        RV32IMemorySize size;
        uint32_t address;
        std::vector<uint8_t> expected_bytes;
    };

    const std::vector<Case> cases{
        {"SB", encodeS(0, 2, 1, 0x0), RV32IMemorySize::Byte, 0x80, {0xdd}},
        {"SH", encodeS(2, 2, 1, 0x1), RV32IMemorySize::Halfword, 0x82, {0xdd, 0xcc}},
        {"SW", encodeS(4, 2, 1, 0x2), RV32IMemorySize::Word, 0x84, {0xdd, 0xcc, 0xbb, 0xaa}},
    };

    for (const auto& test_case : cases) {
        RV32IState state;
        RV32IFunctionalMemory memory;
        state.writeRegister(1, 0x80);
        state.writeRegister(2, 0xaabbccddU);
        const auto trace = stepRaw(test_case.raw, state, memory);
        expectEq(trace.memory.kind, RV32IMemoryAccessKind::Write, std::string(test_case.label) + " memory kind");
        expectEq(trace.memory.size, test_case.size, std::string(test_case.label) + " memory size");
        expectWord(trace.memory.address, test_case.address, std::string(test_case.label) + " address");
        expectWord(trace.memory.write_data, 0xaabbccddU, std::string(test_case.label) + " write data");
        const auto bytes = memory.readBytes(test_case.address, test_case.expected_bytes.size());
        expect(bytes == test_case.expected_bytes, std::string(test_case.label) + " stored bytes");
        expectWord(state.pc, 4, std::string(test_case.label) + " pc after");
    }
}

void testBranchesAndJumps() {
    struct BranchCase {
        const char* label;
        uint8_t funct3;
        uint32_t rs1;
        uint32_t rs2;
        bool taken;
    };

    const std::vector<BranchCase> branches{
        {"BEQ", 0x0, 5, 5, true},
        {"BNE", 0x1, 5, 6, true},
        {"BLT", 0x4, 0xffffffffU, 1, true},
        {"BGE", 0x5, 1, 0xffffffffU, true},
        {"BLTU", 0x6, 1, 2, true},
        {"BGEU", 0x7, 2, 1, true},
        {"BEQ not taken", 0x0, 5, 6, false},
    };

    for (const auto& test_case : branches) {
        RV32IState state;
        state.writeRegister(1, test_case.rs1);
        state.writeRegister(2, test_case.rs2);
        const auto trace = stepRaw(encodeB(8, 2, 1, test_case.funct3), state);
        expectEq(trace.branch_taken, test_case.taken, std::string(test_case.label) + " branch taken");
        expectWord(state.pc, test_case.taken ? 8 : 4, std::string(test_case.label) + " pc after");
    }

    RV32IState jal_state;
    auto jal_trace = stepRaw(encodeJ(12, 5), jal_state);
    expectWriteback(jal_trace, 5, 4, "JAL return address");
    expectWord(jal_state.pc, 12, "JAL target");

    RV32IState jalr_state;
    jalr_state.writeRegister(1, 0x21);
    auto jalr_trace = stepRaw(encodeI(2, 1, 0x0, 5, 0x67), jalr_state);
    expectWriteback(jalr_trace, 5, 4, "JALR return address");
    expectWord(jalr_state.pc, 0x22, "JALR clears bit 0");
}

void testUpperSystemAndTrapBehavior() {
    RV32IState lui_state;
    auto lui_trace = stepRaw(encodeU(0x12345000U, 5, 0x37), lui_state);
    expectWriteback(lui_trace, 5, 0x12345000U, "LUI");

    RV32IState auipc_state;
    RV32IFunctionalMemory auipc_memory;
    auipc_state.pc = 0x20;
    auto auipc_trace = stepRaw(encodeU(0x00001000U, 5, 0x17), auipc_state, auipc_memory);
    expectWriteback(auipc_trace, 5, 0x00001020U, "AUIPC");
    expectWord(auipc_state.pc, 0x24, "AUIPC pc after");

    RV32IState fence_state;
    auto fence_trace = stepRaw(0x0000000fU, fence_state);
    expect(!fence_trace.writeback.enabled, "FENCE has no writeback");
    expectWord(fence_state.pc, 4, "FENCE advances pc");

    RV32IState ecall_state;
    auto ecall_trace = stepRaw(0x00000073U, ecall_state);
    expect(ecall_trace.trapped, "ECALL traps");
    expectEq(ecall_state.trap_cause, RV32IExecutionTrapCause::EnvironmentCall, "ECALL trap cause");
    expectWord(ecall_state.pc, 0, "ECALL keeps pc");

    RV32IState ebreak_state;
    auto ebreak_trace = stepRaw(0x00100073U, ebreak_state);
    expect(ebreak_trace.halted, "EBREAK halts");
    expect(!ebreak_state.trapped, "EBREAK is not a trap in this project model");
    expectWord(ebreak_state.pc, 0, "EBREAK keeps pc");

    RV32IState illegal_state;
    auto illegal_trace = stepRaw(0xffffffffU, illegal_state);
    expect(illegal_trace.trapped, "illegal instruction traps");
    expectEq(illegal_state.trap_cause, RV32IExecutionTrapCause::IllegalInstruction, "illegal trap cause");
}

void testMemoryFaultsAndInstructionLimit() {
    RV32IState fetch_state;
    RV32IFunctionalMemory fetch_memory;
    fetch_state.pc = 2;
    const auto fetch_trace = RV32IInstructionOracle::step(fetch_state, fetch_memory);
    expect(fetch_trace.trapped, "misaligned fetch traps");
    expectEq(fetch_state.trap_cause, RV32IExecutionTrapCause::InstructionAddressMisaligned, "misaligned fetch cause");

    RV32IState fetch_range_state;
    RV32IFunctionalMemory fetch_range_memory(4);
    fetch_range_state.pc = 4;
    const auto fetch_range_trace = RV32IInstructionOracle::step(fetch_range_state, fetch_range_memory);
    expect(fetch_range_trace.trapped, "out-of-range fetch traps");
    expectEq(fetch_range_state.trap_cause, RV32IExecutionTrapCause::InstructionAccessFault, "out-of-range fetch cause");

    RV32IState load_state;
    RV32IFunctionalMemory load_memory;
    load_state.writeRegister(1, 0x80);
    const auto load_trace = stepRaw(encodeI(1, 1, 0x1, 5, 0x03), load_state, load_memory);
    expect(load_trace.memory.fault, "misaligned LH faults memory trace");
    expectEq(load_state.trap_cause, RV32IExecutionTrapCause::LoadAddressMisaligned, "misaligned LH cause");

    RV32IState load_range_state;
    RV32IFunctionalMemory load_range_memory(16);
    load_range_state.writeRegister(1, 16);
    const auto load_range_trace = stepRaw(encodeI(0, 1, 0x2, 5, 0x03), load_range_state, load_range_memory);
    expect(load_range_trace.memory.fault, "out-of-range LW faults memory trace");
    expectEq(load_range_state.trap_cause, RV32IExecutionTrapCause::LoadAccessFault, "out-of-range LW cause");

    RV32IState store_state;
    RV32IFunctionalMemory store_memory(16);
    store_state.writeRegister(1, 16);
    store_state.writeRegister(2, 0xaabbccddU);
    const auto store_trace = stepRaw(encodeS(0, 2, 1, 0x2), store_state, store_memory);
    expect(store_trace.memory.fault, "out-of-range SW faults memory trace");
    expectEq(store_state.trap_cause, RV32IExecutionTrapCause::StoreAccessFault, "out-of-range SW cause");

    RV32IState limit_state;
    RV32IFunctionalMemory limit_memory;
    limit_memory.writeU32(0, 0x0000000fU); // fence, so it would keep running without a limit.
    const auto result = RV32IInstructionOracle::run(limit_state, limit_memory, 0);
    expect(result.instruction_limit_reached, "run reports instruction limit");
    expectEq(result.final_state.trap_cause, RV32IExecutionTrapCause::InstructionLimit, "instruction limit cause");
}
}

std::string RV32IInstructionOracleTest::getTestName() const {
    return "RV32IInstructionOracleTest";
}

void RV32IInstructionOracleTest::verifyResults() {
    testSimpleProgramTrace();
    testRTypeAlu();
    testITypeAlu();
    testLoads();
    testStores();
    testBranchesAndJumps();
    testUpperSystemAndTrapBehavior();
    testMemoryFaultsAndInstructionLimit();
}
