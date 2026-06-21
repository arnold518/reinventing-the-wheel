#include "tests/RV32ISystemTests.hpp"

#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "simulator/Event.hpp"
#include <cassert>
#include <string>
#include <vector>

namespace {
constexpr uint32_t kFence = 0x0000000FU;
constexpr uint32_t kECall = 0x00000073U;
constexpr uint32_t kEBreak = 0x00100073U;
constexpr uint32_t kIllegalInstruction = 0xFFFFFFFFU;

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

uint32_t encodeU(int32_t imm, uint8_t rd, uint8_t opcode) {
    return (static_cast<uint32_t>(imm) & 0xFFFFF000U)
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

uint32_t encodeLUI(int32_t imm, uint8_t rd) {
    return encodeU(imm, rd, 0x37U);
}

uint32_t encodeAUIPC(int32_t imm, uint8_t rd) {
    return encodeU(imm, rd, 0x17U);
}

uint32_t encodeJALR(int32_t imm, uint8_t rs1, uint8_t rd) {
    return encodeI(imm, rs1, 0x0, rd, 0x67U);
}

uint32_t encodeShiftI(uint8_t funct7, uint8_t shamt, uint8_t rs1, uint8_t funct3, uint8_t rd) {
    return encodeI((static_cast<int32_t>(funct7) << 5) | (shamt & 0x1FU), rs1, funct3, rd);
}

void drive(Simulator& sim, size_t time, const std::shared_ptr<Wire<>>& wire, bool value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time, wire, value ? LogicValue::HIGH : LogicValue::LOW));
}

void scheduleClockCycle(Simulator& sim, size_t cycle_start_time, const std::shared_ptr<Wire<>>& clk_wire) {
    drive(sim, cycle_start_time, clk_wire, false);
    drive(sim, cycle_start_time + 1, clk_wire, true);
    drive(sim, cycle_start_time + 5, clk_wire, false);
}

size_t expectedInstructionCount(const RV32ISystemProgramCase& test_case) {
    rv32i::RV32IState state;
    state.pc = test_case.initial_pc;
    for (uint8_t reg = 0; reg < test_case.initial_registers.size(); ++reg) {
        state.writeRegister(reg, test_case.initial_registers[reg]);
    }
    state.forceX0();

    rv32i::RV32IFunctionalMemory memory(test_case.memory_size_bytes);
    memory.loadProgram(test_case.program, test_case.program_base);
    for (const auto& data : test_case.initial_data) {
        memory.loadBytes(data.address, data.bytes);
    }

    size_t instruction_count = 0;
    while (!state.halted && !state.trapped && instruction_count < test_case.max_instructions) {
        rv32i::RV32IInstructionOracle::step(state, memory);
        ++instruction_count;
    }
    return instruction_count;
}

RV32ISystemProgramCase makeProgramCase(const std::string& name,
                                       const std::vector<uint32_t>& words,
                                       size_t max_instructions) {
    RV32ISystemProgramCase test_case;
    test_case.name = name;
    test_case.program = rv32i::RV32IProgram::fromWords(words);
    test_case.max_instructions = max_instructions;
    test_case.max_cycles_per_instruction = 2;
    test_case.cycle_time_step = 10;
    return test_case;
}

uint32_t logicWordToUInt32(const std::vector<LogicValue>& word) {
    assert(word.size() == 32 && "RV32I memory word should contain 32 logic values");
    uint32_t result = 0;
    for (size_t bit = 0; bit < word.size(); ++bit) {
        assert((word[bit] == LogicValue::HIGH || word[bit] == LogicValue::LOW)
               && "RV32I memory word should be known for this check");
        if (word[bit] == LogicValue::HIGH) {
            result |= uint32_t{1} << bit;
        }
    }
    return result;
}
}

void BehavioralRV32ISystemProgramTestBase::setupCircuit() {
    root = Component::create<RV32ISystem>("RV32I_SYSTEM_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

size_t BehavioralRV32ISystemProgramTestBase::getRunDuration() const {
    return visual_run_duration_;
}

void BehavioralRV32ISystemProgramTestBase::buildCircuit() {
    system_ = std::dynamic_pointer_cast<RV32ISystem>(root);
    assert(system_ && "behavioral RV32I system program test requires RV32ISystem root");

    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "behavioral RV32I system program test requires IOComponent root");

    clk_wire_ = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    rst_wire_ = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    enable_wire_ = builder->addNewWire("ENABLE_IN", nullptr, {io_root->getInputPin("ENABLE")});
    builder->addNewWire<32>("PC_OUT", io_root->getOutputPin<32>("PC"), {});
    builder->addNewWire("HALTED_OUT", io_root->getOutputPin("HALTED"), {});
    builder->addNewWire("TRAPPED_OUT", io_root->getOutputPin("TRAPPED"), {});
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram1Test::getCase() const {
    /*
     * C code:
     *   uint32_t input[4] = {3, 5, 7, 11};
     *   uint32_t sum = 0;
     *   for (uint32_t i = 0; i < 4; ++i) sum += input[i];
     *   *(uint32_t *)&memory[0x120] = sum;
     *   memory[0x124] = (uint8_t)sum;
     *   *(uint16_t *)&memory[0x126] = (uint16_t)(sum + 0x34);
     *
     * Assembly:
     *   addi x1, x0, 0x100
     *   addi x2, x0, 4
     *   addi x3, x0, 0
     * loop:
     *   lw   x4, 0(x1)
     *   add  x3, x3, x4
     *   addi x1, x1, 4
     *   addi x2, x2, -1
     *   bne  x2, x0, loop
     *   addi x5, x0, 0x120
     *   sw   x3, 0(x5)
     *   sb   x3, 4(x5)
     *   addi x6, x3, 0x34
     *   sh   x6, 6(x5)
     *   ebreak
     */
    auto test_case = makeProgramCase("BehavioralRV32ISystemProgram1Test", {
        encodeI(0x100, 0, 0x0, 1),       // addi x1, x0, 0x100 ; src pointer
        encodeI(4, 0, 0x0, 2),           // addi x2, x0, 4     ; count
        encodeI(0, 0, 0x0, 3),           // addi x3, x0, 0     ; sum
        encodeI(0, 1, 0x2, 4, 0x03),     // lw   x4, 0(x1)
        encodeR(0x00, 4, 3, 0x0, 3),     // add  x3, x3, x4
        encodeI(4, 1, 0x0, 1),           // addi x1, x1, 4
        encodeI(-1, 2, 0x0, 2),          // addi x2, x2, -1
        encodeB(-16, 0, 2, 0x1),         // bne  x2, x0, loop
        encodeI(0x120, 0, 0x0, 5),       // addi x5, x0, 0x120 ; dst pointer
        encodeS(0, 3, 5, 0x2),           // sw   x3, 0(x5)
        encodeS(4, 3, 5, 0x0),           // sb   x3, 4(x5)
        encodeI(0x34, 3, 0x0, 6),        // addi x6, x3, 0x34
        encodeS(6, 6, 5, 0x1),           // sh   x6, 6(x5)
        kEBreak,                          // ebreak
    }, 40);

    test_case.initial_data.push_back({
        0x100,
        {
            0x03, 0x00, 0x00, 0x00,
            0x05, 0x00, 0x00, 0x00,
            0x07, 0x00, 0x00, 0x00,
            0x0b, 0x00, 0x00, 0x00,
        },
    });
    return test_case;
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram2Test::getCase() const {
    /*
     * C code:
     *   uint32_t a = 0x55, b = 0x0f;
     *   int32_t neg = -8;
     *   uint32_t out[14];
     *   out[0] = a + b;
     *   out[1] = a - b;
     *   out[2] = a & b;
     *   out[3] = a | b;
     *   out[4] = a ^ b;
     *   out[5] = b << 4;
     *   out[6] = out[5] >> 2;
     *   out[7] = (uint32_t)(neg >> 1);
     *   out[8] = neg < (int32_t)a;
     *   out[9] = (uint32_t)neg < a;
     *   out[10] = (a ^ 0x33) | 0x100;
     *   out[11] = a & 0x3f;
     *   out[12] = neg < 0;
     *   out[13] = (uint32_t)neg < 1;
     *   x0 = 123;  // must remain zero.
     *
     * Assembly:
     *   addi x1, x0, 0x55
     *   addi x2, x0, 0x0f
     *   addi x10, x0, -8
     *   add  x3, x1, x2
     *   sub  x4, x1, x2
     *   and  x5, x1, x2
     *   or   x6, x1, x2
     *   xor  x7, x1, x2
     *   slli x8, x2, 4
     *   srli x9, x8, 2
     *   srai x11, x10, 1
     *   slt  x12, x10, x1
     *   sltu x13, x10, x1
     *   xori x14, x1, 0x33
     *   ori  x14, x14, 0x100
     *   andi x15, x1, 0x3f
     *   slti x16, x10, 0
     *   sltiu x17, x10, 1
     *   addi x0, x0, 123
     *   addi x20, x0, 0x200
     *   sw   x3, 0(x20)
     *   sw   x4, 4(x20)
     *   sw   x5, 8(x20)
     *   sw   x6, 12(x20)
     *   sw   x7, 16(x20)
     *   sw   x8, 20(x20)
     *   sw   x9, 24(x20)
     *   sw   x11, 28(x20)
     *   sw   x12, 32(x20)
     *   sw   x13, 36(x20)
     *   sw   x14, 40(x20)
     *   sw   x15, 44(x20)
     *   sw   x16, 48(x20)
     *   sw   x17, 52(x20)
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram2Test", {
        encodeI(0x55, 0, 0x0, 1),             // addi x1, x0, 0x55
        encodeI(0x0f, 0, 0x0, 2),             // addi x2, x0, 0x0f
        encodeI(-8, 0, 0x0, 10),              // addi x10, x0, -8
        encodeR(0x00, 2, 1, 0x0, 3),          // add x3, x1, x2
        encodeR(0x20, 2, 1, 0x0, 4),          // sub x4, x1, x2
        encodeR(0x00, 2, 1, 0x7, 5),          // and x5, x1, x2
        encodeR(0x00, 2, 1, 0x6, 6),          // or x6, x1, x2
        encodeR(0x00, 2, 1, 0x4, 7),          // xor x7, x1, x2
        encodeShiftI(0x00, 4, 2, 0x1, 8),     // slli x8, x2, 4
        encodeShiftI(0x00, 2, 8, 0x5, 9),     // srli x9, x8, 2
        encodeShiftI(0x20, 1, 10, 0x5, 11),   // srai x11, x10, 1
        encodeR(0x00, 1, 10, 0x2, 12),        // slt x12, x10, x1
        encodeR(0x00, 1, 10, 0x3, 13),        // sltu x13, x10, x1
        encodeI(0x33, 1, 0x4, 14),            // xori x14, x1, 0x33
        encodeI(0x100, 14, 0x6, 14),          // ori x14, x14, 0x100
        encodeI(0x3f, 1, 0x7, 15),            // andi x15, x1, 0x3f
        encodeI(0, 10, 0x2, 16),              // slti x16, x10, 0
        encodeI(1, 10, 0x3, 17),              // sltiu x17, x10, 1
        encodeI(123, 0, 0x0, 0),              // addi x0, x0, 123
        encodeI(0x200, 0, 0x0, 20),           // addi x20, x0, 0x200
        encodeS(0, 3, 20, 0x2),               // sw x3, 0(x20)
        encodeS(4, 4, 20, 0x2),               // sw x4, 4(x20)
        encodeS(8, 5, 20, 0x2),               // sw x5, 8(x20)
        encodeS(12, 6, 20, 0x2),              // sw x6, 12(x20)
        encodeS(16, 7, 20, 0x2),              // sw x7, 16(x20)
        encodeS(20, 8, 20, 0x2),              // sw x8, 20(x20)
        encodeS(24, 9, 20, 0x2),              // sw x9, 24(x20)
        encodeS(28, 11, 20, 0x2),             // sw x11, 28(x20)
        encodeS(32, 12, 20, 0x2),             // sw x12, 32(x20)
        encodeS(36, 13, 20, 0x2),             // sw x13, 36(x20)
        encodeS(40, 14, 20, 0x2),             // sw x14, 40(x20)
        encodeS(44, 15, 20, 0x2),             // sw x15, 44(x20)
        encodeS(48, 16, 20, 0x2),             // sw x16, 48(x20)
        encodeS(52, 17, 20, 0x2),             // sw x17, 52(x20)
        kEBreak,                              // ebreak
    }, 48);
}

void BehavioralRV32ISystemProgram2Test::verifyResults() {
    RV32IInstructionLockstepTest::verifyResults();

    auto memory = dataMemoryForTest();
    assert(memory && "BehavioralRV32ISystemProgram2Test requires data memory");

    constexpr uint32_t base_address = 0x00000200U;
    const uint32_t expected_values[] = {
        0x00000064U,
        0x00000046U,
        0x00000005U,
        0x0000005fU,
        0x0000005aU,
        0x000000f0U,
        0x0000003cU,
        0xfffffffcU,
        0x00000001U,
        0x00000000U,
        0x00000166U,
        0x00000015U,
        0x00000001U,
        0x00000000U,
    };
    constexpr size_t expected_count = sizeof(expected_values) / sizeof(expected_values[0]);

    const auto touched = memory->getTouchedWordsAtTime(1000, BehavioralMemory64Kx32::capacityWords());
    assert(touched.size() == expected_count
           && "Program 2 data memory should record every consecutive store as a touched word");

    for (size_t index = 0; index < expected_count; ++index) {
        const auto expected_address = static_cast<uint32_t>(base_address + index * 4);
        assert(touched[index].first == expected_address
               && "Program 2 touched word address should match consecutive store address");
        assert(logicWordToUInt32(touched[index].second) == expected_values[index]
               && "Program 2 touched word value should match stored register value");
    }
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram3Test::getCase() const {
    /*
     * C code:
     *   int32_t sb = (int8_t)memory[0x100];
     *   uint32_t ub = memory[0x100];
     *   int32_t sb2 = (int8_t)memory[0x101];
     *   int32_t sh = (int16_t)*(uint16_t *)&memory[0x104];
     *   uint32_t uh = *(uint16_t *)&memory[0x104];
     *   uint32_t word = *(uint32_t *)&memory[0x108];
     *   *(uint32_t *)&memory[0x120] = (uint32_t)sb;
     *   *(uint32_t *)&memory[0x124] = ub;
     *   *(uint32_t *)&memory[0x128] = (uint32_t)sb2;
     *   *(uint32_t *)&memory[0x12c] = (uint32_t)sh;
     *   *(uint32_t *)&memory[0x130] = uh;
     *   *(uint32_t *)&memory[0x134] = word;
     *   memory[0x138] = (uint8_t)ub;
     *   *(uint16_t *)&memory[0x13a] = (uint16_t)uh;
     *
     * Assembly:
     *   addi x1, x0, 0x100
     *   lb   x2, 0(x1)
     *   lbu  x3, 0(x1)
     *   lb   x4, 1(x1)
     *   lh   x5, 4(x1)
     *   lhu  x6, 4(x1)
     *   lw   x7, 8(x1)
     *   addi x8, x0, 0x120
     *   sw   x2, 0(x8)
     *   sw   x3, 4(x8)
     *   sw   x4, 8(x8)
     *   sw   x5, 12(x8)
     *   sw   x6, 16(x8)
     *   sw   x7, 20(x8)
     *   sb   x3, 24(x8)
     *   sh   x6, 26(x8)
     *   ebreak
     */
    auto test_case = makeProgramCase("BehavioralRV32ISystemProgram3Test", {
        encodeI(0x100, 0, 0x0, 1),       // addi x1, x0, 0x100
        encodeI(0, 1, 0x0, 2, 0x03),     // lb x2, 0(x1)
        encodeI(0, 1, 0x4, 3, 0x03),     // lbu x3, 0(x1)
        encodeI(1, 1, 0x0, 4, 0x03),     // lb x4, 1(x1)
        encodeI(4, 1, 0x1, 5, 0x03),     // lh x5, 4(x1)
        encodeI(4, 1, 0x5, 6, 0x03),     // lhu x6, 4(x1)
        encodeI(8, 1, 0x2, 7, 0x03),     // lw x7, 8(x1)
        encodeI(0x120, 0, 0x0, 8),       // addi x8, x0, 0x120
        encodeS(0, 2, 8, 0x2),           // sw x2, 0(x8)
        encodeS(4, 3, 8, 0x2),           // sw x3, 4(x8)
        encodeS(8, 4, 8, 0x2),           // sw x4, 8(x8)
        encodeS(12, 5, 8, 0x2),          // sw x5, 12(x8)
        encodeS(16, 6, 8, 0x2),          // sw x6, 16(x8)
        encodeS(20, 7, 8, 0x2),          // sw x7, 20(x8)
        encodeS(24, 3, 8, 0x0),          // sb x3, 24(x8)
        encodeS(26, 6, 8, 0x1),          // sh x6, 26(x8)
        kEBreak,                         // ebreak
    }, 24);
    test_case.initial_data.push_back({
        0x100,
        {0x80, 0x7f, 0x00, 0x00, 0x80, 0xff, 0x00, 0x00, 0x21, 0x43, 0x65, 0x87},
    });
    return test_case;
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram4Test::getCase() const {
    /*
     * C code:
     *   uint32_t score = 0;
     *   if (5 == 5) score++;
     *   if (5 != 6) score++;
     *   if (-1 < 1) score++;
     *   if (1 >= -1) score++;
     *   if ((uint32_t)1 < (uint32_t)-1) score++;
     *   if ((uint32_t)-1 >= (uint32_t)1) score++;
     *   if (5 != 1) score++;  // not-taken BEQ fallthrough
     *   *(uint32_t *)&memory[0x120] = score;
     *
     * Assembly:
     *   addi x10, x0, 0
     *   addi x1, x0, 5
     *   addi x2, x0, 5
     *   beq  x1, x2, beq_ok
     *   addi x10, x10, 100
     * beq_ok:
     *   addi x10, x10, 1
     *   addi x2, x0, 6
     *   bne  x1, x2, bne_ok
     *   addi x10, x10, 100
     * bne_ok:
     *   addi x10, x10, 1
     *   addi x3, x0, -1
     *   addi x4, x0, 1
     *   blt  x3, x4, blt_ok
     *   addi x10, x10, 100
     * blt_ok:
     *   addi x10, x10, 1
     *   bge  x4, x3, bge_ok
     *   addi x10, x10, 100
     * bge_ok:
     *   addi x10, x10, 1
     *   bltu x4, x3, bltu_ok
     *   addi x10, x10, 100
     * bltu_ok:
     *   addi x10, x10, 1
     *   bgeu x3, x4, bgeu_ok
     *   addi x10, x10, 100
     * bgeu_ok:
     *   addi x10, x10, 1
     *   beq  x1, x4, wrong_taken
     *   addi x10, x10, 1
     *   jal  x0, store_score
     * wrong_taken:
     *   addi x10, x10, 100
     * store_score:
     *   addi x11, x0, 0x120
     *   sw   x10, 0(x11)
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram4Test", {
        encodeI(0, 0, 0x0, 10),          // addi x10, x0, 0
        encodeI(5, 0, 0x0, 1),           // addi x1, x0, 5
        encodeI(5, 0, 0x0, 2),           // addi x2, x0, 5
        encodeB(8, 2, 1, 0x0),           // beq x1, x2, beq_ok
        encodeI(100, 10, 0x0, 10),       // addi x10, x10, 100
        encodeI(1, 10, 0x0, 10),         // beq_ok: addi x10, x10, 1
        encodeI(6, 0, 0x0, 2),           // addi x2, x0, 6
        encodeB(8, 2, 1, 0x1),           // bne x1, x2, bne_ok
        encodeI(100, 10, 0x0, 10),       // addi x10, x10, 100
        encodeI(1, 10, 0x0, 10),         // bne_ok: addi x10, x10, 1
        encodeI(-1, 0, 0x0, 3),          // addi x3, x0, -1
        encodeI(1, 0, 0x0, 4),           // addi x4, x0, 1
        encodeB(8, 4, 3, 0x4),           // blt x3, x4, blt_ok
        encodeI(100, 10, 0x0, 10),       // addi x10, x10, 100
        encodeI(1, 10, 0x0, 10),         // blt_ok: addi x10, x10, 1
        encodeB(8, 3, 4, 0x5),           // bge x4, x3, bge_ok
        encodeI(100, 10, 0x0, 10),       // addi x10, x10, 100
        encodeI(1, 10, 0x0, 10),         // bge_ok: addi x10, x10, 1
        encodeB(8, 3, 4, 0x6),           // bltu x4, x3, bltu_ok
        encodeI(100, 10, 0x0, 10),       // addi x10, x10, 100
        encodeI(1, 10, 0x0, 10),         // bltu_ok: addi x10, x10, 1
        encodeB(8, 4, 3, 0x7),           // bgeu x3, x4, bgeu_ok
        encodeI(100, 10, 0x0, 10),       // addi x10, x10, 100
        encodeI(1, 10, 0x0, 10),         // bgeu_ok: addi x10, x10, 1
        encodeB(12, 4, 1, 0x0),          // beq x1, x4, wrong_taken
        encodeI(1, 10, 0x0, 10),         // addi x10, x10, 1
        encodeJ(8, 0),                   // jal x0, store_score
        encodeI(100, 10, 0x0, 10),       // wrong_taken: addi x10, x10, 100
        encodeI(0x120, 0, 0x0, 11),      // store_score: addi x11, x0, 0x120
        encodeS(0, 10, 11, 0x2),         // sw x10, 0(x11)
        kEBreak,                         // ebreak
    }, 40);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram5Test::getCase() const {
    /*
     * C code:
     *   uint32_t helper_b(uint32_t value) { return value + 35; }
     *   uint32_t helper_a(void) { uint32_t v = helper_b(7); return v + 1; }
     *   *(uint32_t *)&memory[0x120] = helper_a();
     *
     * Assembly:
     *   addi x1, x0, 0x120
     *   jal  x5, helper_a
     *   sw   x10, 0(x1)
     *   jal  x0, done
     * helper_a:
     *   addi x10, x0, 7
     *   jal  x6, helper_b
     *   addi x10, x10, 1
     *   jalr x0, 0(x5)
     * helper_b:
     *   addi x10, x10, 35
     *   jalr x0, 0(x6)
     * done:
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram5Test", {
        encodeI(0x120, 0, 0x0, 1),       // addi x1, x0, 0x120
        encodeJ(12, 5),                  // jal x5, helper_a
        encodeS(0, 10, 1, 0x2),          // sw x10, 0(x1)
        encodeJ(28, 0),                  // jal x0, done
        encodeI(7, 0, 0x0, 10),          // helper_a: addi x10, x0, 7
        encodeJ(12, 6),                  // jal x6, helper_b
        encodeI(1, 10, 0x0, 10),         // addi x10, x10, 1
        encodeJALR(0, 5, 0),             // jalr x0, 0(x5)
        encodeI(35, 10, 0x0, 10),        // helper_b: addi x10, x10, 35
        encodeJALR(0, 6, 0),             // jalr x0, 0(x6)
        kEBreak,                         // done: ebreak
    }, 20);
}

void BehavioralRV32ISystemProgram5Test::verifyResults() {
    RV32IInstructionLockstepTest::verifyResults();

    auto memory = dataMemoryForTest();
    assert(memory && "BehavioralRV32ISystemProgram5Test requires data memory");

    const auto before_visible_write = memory->getTouchedWordsAtTime(81, BehavioralMemory64Kx32::capacityWords());
    assert(before_visible_write.empty()
           && "Program 5 data memory write must not appear before the delayed write bus is visible");

    const auto at_visible_write = memory->getTouchedWordsAtTime(82, BehavioralMemory64Kx32::capacityWords());
    assert(at_visible_write.size() == 1 && "Program 5 data memory should have one touched word at visible write time");
    assert(at_visible_write[0].first == 0x00000120U && "Program 5 data memory should touch address 0x120");
    assert(logicWordToUInt32(at_visible_write[0].second) == 0x0000002bU
           && "Program 5 data memory should store helper result 43");
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram6Test::getCase() const {
    /*
     * C code:
     *   uint32_t constant = 0x12345678;
     *   uint32_t pc0 = 0x00000008;          // address of auipc x2, 0
     *   uint32_t pc_plus_0x1000 = 0x0000100c; // address of auipc x3, 1 plus 0x1000
     *   *(uint32_t *)&memory[0x120] = constant;
     *   *(uint32_t *)&memory[0x124] = pc0;
     *   *(uint32_t *)&memory[0x128] = pc_plus_0x1000;
     *
     * Assembly:
     *   lui   x1, 0x12345
     *   addi  x1, x1, 0x678
     *   auipc x2, 0
     *   auipc x3, 1
     *   fence
     *   addi x4, x0, 0x120
     *   sw   x1, 0(x4)
     *   sw   x2, 4(x4)
     *   sw   x3, 8(x4)
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram6Test", {
        encodeLUI(0x12345000, 1),        // lui x1, 0x12345
        encodeI(0x678, 1, 0x0, 1),       // addi x1, x1, 0x678
        encodeAUIPC(0, 2),               // auipc x2, 0
        encodeAUIPC(0x1000, 3),          // auipc x3, 1
        kFence,                          // fence
        encodeI(0x120, 0, 0x0, 4),       // addi x4, x0, 0x120
        encodeS(0, 1, 4, 0x2),           // sw x1, 0(x4)
        encodeS(4, 2, 4, 0x2),           // sw x2, 4(x4)
        encodeS(8, 3, 4, 0x2),           // sw x3, 8(x4)
        kEBreak,                         // ebreak
    }, 16);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram7Test::getCase() const {
    /*
     * C code:
     *   uint32_t a = 0, b = 1;
     *   for (uint32_t i = 0; i < 8; ++i) {
     *       *(uint32_t *)&memory[0x120 + i * 4] = a;
     *       uint32_t next = a + b;
     *       a = b;
     *       b = next;
     *   }
     *
     * Assembly:
     *   addi x1, x0, 0x120
     *   addi x2, x0, 8
     *   addi x3, x0, 0
     *   addi x4, x0, 1
     * fib_loop:
     *   sw   x3, 0(x1)
     *   add  x5, x3, x4
     *   addi x3, x4, 0
     *   addi x4, x5, 0
     *   addi x1, x1, 4
     *   addi x2, x2, -1
     *   bne  x2, x0, fib_loop
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram7Test", {
        encodeI(0x120, 0, 0x0, 1),       // addi x1, x0, 0x120
        encodeI(8, 0, 0x0, 2),           // addi x2, x0, 8
        encodeI(0, 0, 0x0, 3),           // addi x3, x0, 0
        encodeI(1, 0, 0x0, 4),           // addi x4, x0, 1
        encodeS(0, 3, 1, 0x2),           // fib_loop: sw x3, 0(x1)
        encodeR(0x00, 4, 3, 0x0, 5),     // add x5, x3, x4
        encodeI(0, 4, 0x0, 3),           // addi x3, x4, 0
        encodeI(0, 5, 0x0, 4),           // addi x4, x5, 0
        encodeI(4, 1, 0x0, 1),           // addi x1, x1, 4
        encodeI(-1, 2, 0x0, 2),          // addi x2, x2, -1
        encodeB(-24, 0, 2, 0x1),         // bne x2, x0, fib_loop
        kEBreak,                         // ebreak
    }, 80);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram8Test::getCase() const {
    /*
     * C code:
     *   uint32_t sum = 0;
     *   for (uint32_t i = 0; i < 5; ++i) {
     *       uint8_t value = memory[0x100 + i];
     *       memory[0x140 + i] = value;
     *       sum += value;
     *   }
     *   *(uint32_t *)&memory[0x160] = sum;
     *
     * Assembly:
     *   addi x1, x0, 0x100
     *   addi x2, x0, 0x140
     *   addi x3, x0, 5
     *   addi x4, x0, 0
     * copy_loop:
     *   lbu  x5, 0(x1)
     *   sb   x5, 0(x2)
     *   add  x4, x4, x5
     *   addi x1, x1, 1
     *   addi x2, x2, 1
     *   addi x3, x3, -1
     *   bne  x3, x0, copy_loop
     *   addi x6, x0, 0x160
     *   sw   x4, 0(x6)
     *   ebreak
     */
    auto test_case = makeProgramCase("BehavioralRV32ISystemProgram8Test", {
        encodeI(0x100, 0, 0x0, 1),       // addi x1, x0, 0x100
        encodeI(0x140, 0, 0x0, 2),       // addi x2, x0, 0x140
        encodeI(5, 0, 0x0, 3),           // addi x3, x0, 5
        encodeI(0, 0, 0x0, 4),           // addi x4, x0, 0
        encodeI(0, 1, 0x4, 5, 0x03),     // copy_loop: lbu x5, 0(x1)
        encodeS(0, 5, 2, 0x0),           // sb x5, 0(x2)
        encodeR(0x00, 5, 4, 0x0, 4),     // add x4, x4, x5
        encodeI(1, 1, 0x0, 1),           // addi x1, x1, 1
        encodeI(1, 2, 0x0, 2),           // addi x2, x2, 1
        encodeI(-1, 3, 0x0, 3),          // addi x3, x3, -1
        encodeB(-24, 0, 3, 0x1),         // bne x3, x0, copy_loop
        encodeI(0x160, 0, 0x0, 6),       // addi x6, x0, 0x160
        encodeS(0, 4, 6, 0x2),           // sw x4, 0(x6)
        kEBreak,                         // ebreak
    }, 60);
    test_case.initial_data.push_back({0x100, {0x01, 0x02, 0x03, 0x04, 0xff}});
    return test_case;
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram9Test::getCase() const {
    /*
     * C code:
     *   x1 = 1;
     *   execute an invalid RV32I instruction and trap;
     *   x2 = 2 must not execute.
     *
     * Assembly:
     *   addi x1, x0, 1
     *   .word 0xffffffff
     *   addi x2, x0, 2
     */
    return makeProgramCase("BehavioralRV32ISystemProgram9Test", {
        encodeI(1, 0, 0x0, 1),           // addi x1, x0, 1
        kIllegalInstruction,             // illegal instruction
        encodeI(2, 0, 0x0, 2),           // addi x2, x0, 2
    }, 4);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram10Test::getCase() const {
    /*
     * C code:
     *   x1 = 1;
     *   ecall traps;
     *   x2 = 2 must not execute.
     *
     * Assembly:
     *   addi x1, x0, 1
     *   ecall
     *   addi x2, x0, 2
     */
    return makeProgramCase("BehavioralRV32ISystemProgram10Test", {
        encodeI(1, 0, 0x0, 1),           // addi x1, x0, 1
        kECall,                          // ecall
        encodeI(2, 0, 0x0, 2),           // addi x2, x0, 2
    }, 4);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram11Test::getCase() const {
    /*
     * C code:
     *   volatile uint32_t value = *(volatile uint32_t *)0x101;
     *   The misaligned word load traps and does not write x2.
     *
     * Assembly:
     *   addi x1, x0, 0x101
     *   lw   x2, 0(x1)
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram11Test", {
        encodeI(0x101, 0, 0x0, 1),       // addi x1, x0, 0x101
        encodeI(0, 1, 0x2, 2, 0x03),     // lw x2, 0(x1)
        kEBreak,                         // ebreak; must not execute
    }, 4);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram12Test::getCase() const {
    /*
     * C code:
     *   *(volatile uint16_t *)0x101 = 0x123;
     *   The misaligned halfword store traps and writes no bytes.
     *
     * Assembly:
     *   addi x1, x0, 0x101
     *   addi x2, x0, 0x123
     *   sh   x2, 0(x1)
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram12Test", {
        encodeI(0x101, 0, 0x0, 1),       // addi x1, x0, 0x101
        encodeI(0x123, 0, 0x0, 2),       // addi x2, x0, 0x123
        encodeS(0, 2, 1, 0x1),           // sh x2, 0(x1)
        kEBreak,                         // ebreak; must not execute
    }, 5);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram13Test::getCase() const {
    /*
     * C code:
     *   volatile uint32_t value = *(volatile uint32_t *)0x00040000;
     *   Address 0x00040000 is just outside the implemented 256 KiB memory.
     *
     * Assembly:
     *   lui  x1, 0x40
     *   lw   x2, 0(x1)
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram13Test", {
        encodeLUI(0x00040000, 1),        // lui x1, 0x40
        encodeI(0, 1, 0x2, 2, 0x03),     // lw x2, 0(x1)
        kEBreak,                         // ebreak; must not execute
    }, 4);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram14Test::getCase() const {
    /*
     * C code:
     *   *(volatile uint32_t *)0x00040000 = 0x7b;
     *   The out-of-range store traps and writes no bytes.
     *
     * Assembly:
     *   lui  x1, 0x40
     *   addi x2, x0, 0x7b
     *   sw   x2, 0(x1)
     *   ebreak
     */
    return makeProgramCase("BehavioralRV32ISystemProgram14Test", {
        encodeLUI(0x00040000, 1),        // lui x1, 0x40
        encodeI(0x7b, 0, 0x0, 2),        // addi x2, x0, 0x7b
        encodeS(0, 2, 1, 0x2),           // sw x2, 0(x1)
        kEBreak,                         // ebreak; must not execute
    }, 5);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram15Test::getCase() const {
    /*
     * C code:
     *   ((void (*)(void))2)();
     *   Jump to byte address 2; the next attempted fetch traps because PC is not word-aligned.
     *
     * Assembly:
     *   addi x1, x0, 2
     *   jalr x0, 0(x1)
     */
    return makeProgramCase("BehavioralRV32ISystemProgram15Test", {
        encodeI(2, 0, 0x0, 1),           // addi x1, x0, 2
        encodeJALR(0, 1, 0),             // jalr x0, 0(x1)
    }, 4);
}

RV32ISystemProgramCase BehavioralRV32ISystemProgram16Test::getCase() const {
    /*
     * C code:
     *   ((void (*)(void))0x00040000)();
     *   Jump to the first byte outside implemented instruction memory; the next fetch traps.
     *
     * Assembly:
     *   lui  x1, 0x40
     *   jalr x0, 0(x1)
     */
    return makeProgramCase("BehavioralRV32ISystemProgram16Test", {
        encodeLUI(0x00040000, 1),        // lui x1, 0x40
        encodeJALR(0, 1, 0),             // jalr x0, 0(x1)
    }, 4);
}

void BehavioralRV32ISystemProgramTestBase::initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) {
    assert(system_ && "behavioral RV32I system program test system is not initialized");

    system_->clearInstructionMemory();
    system_->clearDataMemory();
    system_->setInitialPC(test_case.initial_pc);
    system_->loadProgram(test_case.program, test_case.program_base);
    for (uint8_t reg = 0; reg < test_case.initial_registers.size(); ++reg) {
        system_->setRegister(reg, test_case.initial_registers[reg]);
    }
    for (const auto& data : test_case.initial_data) {
        system_->loadDataBytes(data.address, data.bytes);
    }

    drive(*sim, 0, clk_wire_, false);
    drive(*sim, 0, rst_wire_, false);
    drive(*sim, 0, enable_wire_, true);

    const size_t cycle_count = expectedInstructionCount(test_case);
    visual_run_duration_ = cycle_count * test_case.cycle_time_step;
    for (size_t cycle = 0; cycle < cycle_count; ++cycle) {
        scheduleClockCycle(*sim, cycle * test_case.cycle_time_step, clk_wire_);
    }
}

void BehavioralRV32ISystemProgramTestBase::clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) {
    (void)cycle_index;
    (void)cycle_start_time;
}

rv32i::RV32IState BehavioralRV32ISystemProgramTestBase::snapshotComponentState() const {
    assert(system_ && "behavioral RV32I system program test system is not initialized");
    return system_->snapshotState();
}

rv32i::RV32IMemoryTrace BehavioralRV32ISystemProgramTestBase::lastDataMemoryAccess() const {
    assert(system_ && "behavioral RV32I system program test system is not initialized");
    return system_->lastDataMemoryAccess();
}

std::map<uint32_t, uint8_t> BehavioralRV32ISystemProgramTestBase::lastDataMemoryWrites() const {
    assert(system_ && "behavioral RV32I system program test system is not initialized");
    return system_->lastDataMemoryWrites();
}

std::shared_ptr<BehavioralMemory64Kx32> BehavioralRV32ISystemProgramTestBase::dataMemoryForTest() const {
    assert(system_ && "behavioral RV32I system program test system is not initialized");
    return system_->dataMemory();
}
