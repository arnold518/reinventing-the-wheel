#include "tests/RV32IBlockEquivalenceTests.hpp"

#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/IOComponent.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/composite/ALU32.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "rv32i/RV32IState.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::shared_ptr<IOComponent> createFamilyDut(
    const circuit::ComponentFamily& family,
    circuit::Fidelity fidelity) {
    auto profile = circuit::BuildProfileBuilder(
        "equivalence-" + circuit::toString(fidelity))
        .addRule(circuit::preferFidelity(
            fidelity,
            circuit::ProfileSelector::exactPath("DUT"),
            "equivalence test selects one family fidelity"))
        .build();
    auto root = circuit::builtinComponentCatalog()
        .createRoot(family.request("DUT"), std::move(profile))
        .root;
    if (root->getSelectedFidelity() != circuit::toString(fidelity)) {
        throw std::runtime_error(
            "Equivalence test did not instantiate the requested fidelity");
    }
    auto io = std::dynamic_pointer_cast<IOComponent>(root);
    if (!io) {
        throw std::runtime_error(
            "Equivalence family root is not an IOComponent");
    }
    return io;
}

template<size_t Width>
void drive(Simulator& sim, size_t time, const std::shared_ptr<Wire<Width>>& wire, uint64_t value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<Width>>(time, wire, value));
}

template<size_t Width>
void driveVector(Simulator& sim,
                 size_t time,
                 const std::shared_ptr<Wire<Width>>& wire,
                 const std::vector<LogicValue>& value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<Width>>(time, wire, value));
}

void driveBit(Simulator& sim, size_t time, const std::shared_ptr<Wire<>>& wire, bool value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time, wire, value ? LogicValue::HIGH : LogicValue::LOW));
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct ControlFlowExpected {
    size_t time;
    uint32_t pc;
    uint32_t pc_plus_4;
    uint32_t next_pc;
    bool branch_taken;
    bool pc_misaligned;
    bool target_misaligned;
    const char* label;
};

const std::vector<ControlFlowExpected>& controlFlowExpected() {
    static const std::vector<ControlFlowExpected> values{
        {900, 0x00000000U, 0x00000004U, 0x00000004U, false, false, false, "reset"},
        {1900, 0x00000000U, 0x00000004U, 0x00000004U, false, false, false, "reset released"},
        {2900, 0x00000004U, 0x00000008U, 0x00000008U, false, false, false, "PC+4 write"},
        {3900, 0x00000004U, 0x00000008U, 0x00000008U, false, false, false, "PC hold"},
        {4900, 0x0000000cU, 0x00000010U, 0x00000014U, true, false, false, "taken BEQ"},
        {5900, 0x00000010U, 0x00000014U, 0x00000014U, false, false, false, "not-taken misaligned branch target ignored"},
        {6900, 0x00000018U, 0x0000001cU, 0x00000020U, false, false, false, "JAL"},
        {7400, 0x00000018U, 0x0000001cU, 0x00000102U, false, false, true, "misaligned JALR candidate held"},
        {7900, 0x00000018U, 0x0000001cU, 0x00000104U, false, false, false, "aligned JALR candidate"},
        {8900, 0x00000102U, 0x00000106U, 0x00000102U, false, true, true, "misaligned PC visible"},
        {9900, 0x00000000U, 0x00000004U, 0x00000004U, false, false, false, "reset recovery"},
        {10400, 0x00000000U, 0x00000004U, 0x00000000U, true, false, false, "BNE decision"},
        {10900, 0x00000000U, 0x00000004U, 0x00000000U, true, false, false, "BLT decision"},
        {11400, 0x00000000U, 0x00000004U, 0x00000000U, true, false, false, "BGE decision"},
        {11900, 0x00000000U, 0x00000004U, 0x00000000U, true, false, false, "BLTU decision"},
        {12400, 0x00000000U, 0x00000004U, 0x00000000U, true, false, false, "BGEU decision"},
    };
    return values;
}

constexpr size_t CONTROL_FLOW_RANDOM_START = 12500;
constexpr size_t CONTROL_FLOW_RANDOM_STEP = 500;

struct ControlFlowRandomCase {
    uint32_t rs1;
    uint32_t immediate;
    uint8_t branch_type;
    uint8_t jump_type;
    bool eq;
    bool lt_signed;
    bool lt_unsigned;
    std::string label;
};

const std::vector<ControlFlowRandomCase>& controlFlowRandomCases() {
    static const std::vector<ControlFlowRandomCase> values = [] {
        std::vector<ControlFlowRandomCase> result;
        uint32_t state = 0xC0F10F01U;
        for (size_t index = 0; index < 64; ++index) {
            state = state * 1664525U + 1013904223U;
            const uint32_t rs1 = state;
            state = state * 1664525U + 1013904223U;
            const uint32_t immediate = state;
            state = state * 1664525U + 1013904223U;
            result.push_back({rs1,
                              immediate,
                              static_cast<uint8_t>((state >> 3U) & 0x7U),
                              static_cast<uint8_t>((state >> 7U) & 0x3U),
                              (state & 0x1U) != 0,
                              (state & 0x2U) != 0,
                              (state & 0x4U) != 0,
                              "deterministic random " + std::to_string(index)});
        }
        return result;
    }();
    return values;
}

ControlFlowExpected expectedRandomControlFlow(const ControlFlowRandomCase& test_case,
                                              size_t time) {
    bool branch_taken = false;
    switch (test_case.branch_type) {
        case 1: branch_taken = test_case.eq; break;
        case 2: branch_taken = !test_case.eq; break;
        case 3: branch_taken = test_case.lt_signed; break;
        case 4: branch_taken = !test_case.lt_signed; break;
        case 5: branch_taken = test_case.lt_unsigned; break;
        case 6: branch_taken = !test_case.lt_unsigned; break;
        default: branch_taken = false; break;
    }

    uint32_t next = branch_taken ? test_case.immediate : 4U;
    if (test_case.jump_type == 1) {
        next = test_case.immediate;
    } else if (test_case.jump_type == 2) {
        next = (test_case.rs1 + test_case.immediate) & ~uint32_t{1};
    } else if (test_case.jump_type == 3) {
        next = 0;
    }
    const bool active_transfer = branch_taken || test_case.jump_type != 0;
    return {time, 0, 4, next, branch_taken, false,
            active_transfer && (next & 0x3U) != 0,
            test_case.label.c_str()};
}

void checkControlFlowComponent(const std::shared_ptr<IOComponent>& component,
                               const ControlFlowExpected& expected,
                               const std::string& implementation) {
    const auto prefix = implementation + " " + expected.label + ": ";
    require(component->getOutputPin<32>("PC")->getValueAsUInt64() == expected.pc, prefix + "PC");
    require(component->getOutputPin<32>("PC_PLUS_4")->getValueAsUInt64() == expected.pc_plus_4, prefix + "PC_PLUS_4");
    require(component->getOutputPin<32>("NEXT_PC_CANDIDATE")->getValueAsUInt64() == expected.next_pc, prefix + "NEXT_PC_CANDIDATE");
    require(component->getOutputPin("BRANCH_TAKEN")->getValue() == (expected.branch_taken ? LogicValue::HIGH : LogicValue::LOW), prefix + "BRANCH_TAKEN");
    require(component->getOutputPin("PC_MISALIGNED")->getValue() == (expected.pc_misaligned ? LogicValue::HIGH : LogicValue::LOW), prefix + "PC_MISALIGNED");
    require(component->getOutputPin("TARGET_MISALIGNED")->getValue() == (expected.target_misaligned ? LogicValue::HIGH : LogicValue::LOW), prefix + "TARGET_MISALIGNED");
}

constexpr size_t DECODE_STEP = 1000;

struct DecodeCase {
    uint32_t raw;
    std::string label;
};

uint32_t encodeR(uint8_t funct7, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t rd) {
    return (static_cast<uint32_t>(funct7) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | 0x33U;
}

uint32_t encodeI(int32_t immediate, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
    return ((static_cast<uint32_t>(immediate) & 0xFFFU) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | opcode;
}

uint32_t encodeS(int32_t immediate, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t bits = static_cast<uint32_t>(immediate) & 0xFFFU;
    return ((bits >> 5U) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | ((bits & 0x1FU) << 7U)
         | 0x23U;
}

uint32_t encodeB(int32_t immediate, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t bits = static_cast<uint32_t>(immediate) & 0x1FFFU;
    return (((bits >> 12U) & 1U) << 31U)
         | (((bits >> 5U) & 0x3FU) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (((bits >> 1U) & 0xFU) << 8U)
         | (((bits >> 11U) & 1U) << 7U)
         | 0x63U;
}

uint32_t encodeU(uint32_t immediate, uint8_t rd, uint8_t opcode) {
    return (immediate & 0xFFFFF000U) | (static_cast<uint32_t>(rd) << 7U) | opcode;
}

uint32_t encodeJ(int32_t immediate, uint8_t rd) {
    const uint32_t bits = static_cast<uint32_t>(immediate) & 0x1FFFFFU;
    return (((bits >> 20U) & 1U) << 31U)
         | (((bits >> 1U) & 0x3FFU) << 21U)
         | (((bits >> 11U) & 1U) << 20U)
         | (((bits >> 12U) & 0xFFU) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | 0x6FU;
}

const std::vector<DecodeCase>& decodeCases() {
    static const std::vector<DecodeCase> values = [] {
        std::vector<DecodeCase> result{
        {encodeR(0x00, 7, 6, 0x0, 5), "ADD"},
        {encodeR(0x20, 7, 6, 0x0, 5), "SUB"},
        {encodeR(0x00, 7, 6, 0x1, 5), "SLL"},
        {encodeR(0x00, 7, 6, 0x2, 5), "SLT"},
        {encodeR(0x00, 7, 6, 0x3, 5), "SLTU"},
        {encodeR(0x00, 7, 6, 0x4, 5), "XOR"},
        {encodeR(0x00, 7, 6, 0x5, 5), "SRL"},
        {encodeR(0x20, 7, 6, 0x5, 5), "SRA"},
        {encodeR(0x00, 7, 6, 0x6, 5), "OR"},
        {encodeR(0x00, 7, 6, 0x7, 5), "AND"},

        {encodeI(-17, 6, 0x0, 5, 0x13), "ADDI"},
        {encodeI(-17, 6, 0x2, 5, 0x13), "SLTI"},
        {encodeI(-17, 6, 0x3, 5, 0x13), "SLTIU"},
        {encodeI(0x155, 6, 0x4, 5, 0x13), "XORI"},
        {encodeI(0x155, 6, 0x6, 5, 0x13), "ORI"},
        {encodeI(0x155, 6, 0x7, 5, 0x13), "ANDI"},
        {encodeI(31, 6, 0x1, 5, 0x13), "SLLI"},
        {encodeI(31, 6, 0x5, 5, 0x13), "SRLI"},
        {encodeI((0x20 << 5) | 31, 6, 0x5, 5, 0x13), "SRAI"},

        {encodeI(-12, 6, 0x0, 5, 0x03), "LB"},
        {encodeI(-12, 6, 0x1, 5, 0x03), "LH"},
        {encodeI(-12, 6, 0x2, 5, 0x03), "LW"},
        {encodeI(-12, 6, 0x4, 5, 0x03), "LBU"},
        {encodeI(-12, 6, 0x5, 5, 0x03), "LHU"},
        {encodeS(-20, 7, 6, 0x0), "SB"},
        {encodeS(-20, 7, 6, 0x1), "SH"},
        {encodeS(-20, 7, 6, 0x2), "SW"},

        {encodeB(-16, 7, 6, 0x0), "BEQ"},
        {encodeB(-16, 7, 6, 0x1), "BNE"},
        {encodeB(-16, 7, 6, 0x4), "BLT"},
        {encodeB(-16, 7, 6, 0x5), "BGE"},
        {encodeB(-16, 7, 6, 0x6), "BLTU"},
        {encodeB(-16, 7, 6, 0x7), "BGEU"},

        {encodeJ(-2048, 5), "JAL"},
        {encodeI(-4, 6, 0x0, 5, 0x67), "JALR"},
        {encodeU(0xABCDE000U, 5, 0x37), "LUI"},
        {encodeU(0x12345000U, 5, 0x17), "AUIPC"},
        {encodeI(0x123, 4, 0x0, 0, 0x0F), "FENCE"},
        {0x00000073U, "ECALL"},
        {0x00100073U, "EBREAK"},

        {0x00000000U, "invalid length"},
        {0x0000007FU, "unknown opcode"},
        {encodeI(0, 6, 0x1, 5, 0x67), "invalid JALR funct3"},
        {encodeR(0x01, 7, 6, 0x0, 5), "invalid R funct7"},
        {encodeI(0, 0, 0x1, 0, 0x0F), "unsupported FENCE.I"},
        {encodeI(0, 6, 0x0, 5, 0x1B), "unsupported RV64 OP-IMM-32"},
        };
        uint32_t state = 0xDEC0DE02U;
        for (size_t index = 0; index < 64; ++index) {
            state = state * 1664525U + 1013904223U;
            result.push_back({state, "constrained-random raw " + std::to_string(index)});
        }
        return result;
    }();
    return values;
}

void checkDecodeBus(const std::shared_ptr<IOComponent>& component,
                    const char* pin,
                    uint64_t expected,
                    const std::string& prefix) {
    require(component->getOutputPinDynamic(pin)->getValueAsUInt64() == expected,
            prefix + pin);
}

void checkDecodeBit(const std::shared_ptr<IOComponent>& component,
                    const char* pin,
                    bool expected,
                    const std::string& prefix) {
    require(component->getOutputPin(pin)->getValue() == (expected ? LogicValue::HIGH : LogicValue::LOW),
            prefix + pin);
}

void checkDecodeComponent(const std::shared_ptr<IOComponent>& component,
                          const DecodeCase& test_case,
                          const std::string& implementation) {
    const auto decoded = rv32i::RV32IDecoder::decode(test_case.raw);
    const auto control = rv32i::RV32IControl::fromDecoded(decoded);
    const std::string prefix = implementation + " " + test_case.label + ": ";

    checkDecodeBus(component, "RS1_ADDR", decoded.rs1, prefix);
    checkDecodeBus(component, "RS2_ADDR", decoded.rs2, prefix);
    checkDecodeBus(component, "RD_ADDR", decoded.rd, prefix);
    checkDecodeBus(component, "IMM", static_cast<uint32_t>(decoded.immediate), prefix);
    checkDecodeBus(component, "ALU_OP", control.alu_op, prefix);
    checkDecodeBus(component, "ALU_A_SEL", rv32i::component_encoding::aluSourceA(control.alu_a), prefix);
    checkDecodeBus(component, "ALU_B_SEL", rv32i::component_encoding::aluSourceB(control.alu_b), prefix);
    checkDecodeBit(component, "LEGAL", control.legal, prefix);
    checkDecodeBit(component, "REG_WRITE", control.reg_write, prefix);
    checkDecodeBit(component, "MEM_READ", control.mem_read, prefix);
    checkDecodeBit(component, "MEM_WRITE", control.mem_write, prefix);
    checkDecodeBus(component, "WRITEBACK_SEL", rv32i::component_encoding::writeback(control.writeback), prefix);
    checkDecodeBus(component, "MEM_SIZE", rv32i::component_encoding::memorySize(control.mem_size), prefix);
    checkDecodeBit(component, "LOAD_SIGN_EXTEND", control.load_sign_extend, prefix);
    checkDecodeBus(component, "BRANCH_TYPE", rv32i::component_encoding::branch(control.branch), prefix);
    checkDecodeBus(component, "JUMP_TYPE", rv32i::component_encoding::jump(control.jump), prefix);
    checkDecodeBit(component, "HALT_REQUEST", control.halt, prefix);
    checkDecodeBit(component, "TRAP_REQUEST", control.trap, prefix);
    checkDecodeBus(component, "DECODE_TRAP_CAUSE", rv32i::component_encoding::decodeTrapCause(control.trap_cause), prefix);
}

constexpr size_t REGISTER_FILE_STEP = 1000;

uint32_t registerPattern(size_t index) {
    return index == 0 ? 0U : 0x10203040U ^ (static_cast<uint32_t>(index) * 0x01010101U);
}

struct RegisterFileExpected {
    size_t time;
    uint32_t rs1;
    uint32_t rs2;
    const char* label;
};

std::vector<RegisterFileExpected> registerFileExpected() {
    std::vector<RegisterFileExpected> values{{900, 0, 0, "reset clears both read ports"}};
    for (size_t reg = 0; reg < 32; ++reg) {
        values.push_back({2000 + reg * REGISTER_FILE_STEP + 900,
                          registerPattern(reg),
                          registerPattern(reg == 0 ? 0 : reg - 1),
                          "write/read register"});
    }
    values.push_back({34000 + 900, registerPattern(5), registerPattern(31), "disabled write is ignored"});
    values.push_back({35000 + 900, 0, 0, "reset recovery"});
    return values;
}

void checkRegisterFileComponent(const std::shared_ptr<IOComponent>& component,
                                const RegisterFileExpected& expected,
                                const std::string& implementation) {
    const auto prefix = implementation + " " + expected.label + ": ";
    require(component->getOutputPin<32>("RS1_DATA")->getValueAsUInt64() == expected.rs1,
            prefix + "RS1_DATA");
    require(component->getOutputPin<32>("RS2_DATA")->getValueAsUInt64() == expected.rs2,
            prefix + "RS2_DATA");
}

constexpr size_t ALU_STEP = 1000;

struct ALUCase {
    uint32_t a;
    uint32_t b;
    uint8_t op;
    std::string label;
};

struct ALUExpected {
    uint32_t out;
    bool zero;
    bool eq;
    bool lt_signed;
    bool lt_unsigned;
    bool negative;
    bool carry;
    bool overflow;
};

uint32_t expectedArithmeticShiftRight(uint32_t value, uint32_t amount) {
    amount &= 0x1FU;
    if (amount == 0) return value;
    const uint32_t shifted = value >> amount;
    return (value & 0x80000000U) == 0
        ? shifted
        : shifted | (~uint32_t{0} << (32U - amount));
}

ALUExpected expectedALU(const ALUCase& test_case) {
    const uint32_t a = test_case.a;
    const uint32_t b = test_case.b;
    uint32_t out = 0;
    bool carry = false;
    bool overflow = false;
    switch (test_case.op) {
        case ALU32Op::ADD: {
            const uint64_t wide = static_cast<uint64_t>(a) + static_cast<uint64_t>(b);
            out = static_cast<uint32_t>(wide);
            carry = wide > 0xFFFFFFFFULL;
            overflow = (~(a ^ b) & (a ^ out) & 0x80000000U) != 0;
            break;
        }
        case ALU32Op::SUB:
            out = a - b;
            carry = a >= b;
            overflow = ((a ^ b) & (a ^ out) & 0x80000000U) != 0;
            break;
        case ALU32Op::AND: out = a & b; break;
        case ALU32Op::OR: out = a | b; break;
        case ALU32Op::XOR: out = a ^ b; break;
        case ALU32Op::SLL: out = a << (b & 0x1FU); break;
        case ALU32Op::SRL: out = a >> (b & 0x1FU); break;
        case ALU32Op::SRA: out = expectedArithmeticShiftRight(a, b); break;
        case ALU32Op::SLT:
            out = static_cast<int32_t>(a) < static_cast<int32_t>(b) ? 1U : 0U;
            carry = a >= b;
            overflow = ((a ^ b) & (a ^ (a - b)) & 0x80000000U) != 0;
            break;
        case ALU32Op::SLTU:
            out = a < b ? 1U : 0U;
            carry = a >= b;
            overflow = ((a ^ b) & (a ^ (a - b)) & 0x80000000U) != 0;
            break;
        case ALU32Op::PASS_A: out = a; break;
        case ALU32Op::PASS_B: out = b; break;
        case ALU32Op::ZERO: out = 0; break;
        default: out = 0; break;
    }
    return {out,
            out == 0,
            a == b,
            static_cast<int32_t>(a) < static_cast<int32_t>(b),
            a < b,
            (out & 0x80000000U) != 0,
            carry,
            overflow};
}

const std::vector<ALUCase>& aluCases() {
    static const std::vector<ALUCase> values = [] {
        std::vector<ALUCase> result;
        for (uint8_t op = 0; op < 32; ++op) {
            result.push_back({0x12345678U ^ (static_cast<uint32_t>(op) * 0x01010101U),
                              0x9ABCDEF0U ^ (static_cast<uint32_t>(op) * 0x11111111U),
                              op,
                              "operation " + std::to_string(op)});
        }

        for (const auto& value : std::array<ALUCase, 32>{{
                 {0xFFFFFFFFU, 1, ALU32Op::ADD, "ADD carry"},
                 {0x7FFFFFFFU, 1, ALU32Op::ADD, "ADD positive overflow"},
                 {0x80000000U, 0x80000000U, ALU32Op::ADD, "ADD negative overflow"},
                 {0, 0, ALU32Op::ADD, "ADD zero"},
                 {0, 1, ALU32Op::SUB, "SUB borrow"},
                 {0x80000000U, 1, ALU32Op::SUB, "SUB negative no overflow"},
                 {0x80000000U, 0x7FFFFFFFU, ALU32Op::SUB, "SUB overflow"},
                 {5, 5, ALU32Op::SUB, "SUB equality"},
                 {0xF0F0F0F0U, 0x0FF00FF0U, ALU32Op::AND, "AND pattern"},
                 {0xF0F0F0F0U, 0x0FF00FF0U, ALU32Op::OR, "OR pattern"},
                 {0xF0F0F0F0U, 0x0FF00FF0U, ALU32Op::XOR, "XOR pattern"},
                 {1, 0, ALU32Op::SLL, "SLL zero"},
                 {1, 31, ALU32Op::SLL, "SLL 31"},
                 {1, 32, ALU32Op::SLL, "SLL masks 32"},
                 {1, 33, ALU32Op::SLL, "SLL masks 33"},
                 {0x80000000U, 0, ALU32Op::SRL, "SRL zero"},
                 {0x80000000U, 31, ALU32Op::SRL, "SRL 31"},
                 {0x80000000U, 32, ALU32Op::SRL, "SRL masks 32"},
                 {0x80000000U, 33, ALU32Op::SRL, "SRL masks 33"},
                 {0x80000000U, 0, ALU32Op::SRA, "SRA zero"},
                 {0x80000000U, 31, ALU32Op::SRA, "SRA 31"},
                 {0x80000000U, 32, ALU32Op::SRA, "SRA masks 32"},
                 {0x80000000U, 33, ALU32Op::SRA, "SRA masks 33"},
                 {0xFFFFFFFFU, 1, ALU32Op::SLT, "SLT signed negative"},
                 {0x7FFFFFFFU, 0x80000000U, ALU32Op::SLT, "SLT signed boundary"},
                 {0xFFFFFFFFU, 1, ALU32Op::SLTU, "SLTU unsigned high"},
                 {0, 0xFFFFFFFFU, ALU32Op::SLTU, "SLTU unsigned low"},
                 {0x80000000U, 0x7FFFFFFFU, ALU32Op::PASS_A, "PASS_A"},
                 {0x80000000U, 0x7FFFFFFFU, ALU32Op::PASS_B, "PASS_B"},
                 {0x80000000U, 0x7FFFFFFFU, ALU32Op::ZERO, "ZERO"},
                 {0xAAAAAAAAU, 0xAAAAAAAAU, ALU32Op::XOR, "EQ and result zero"},
                 {0x80000000U, 0, ALU32Op::OR, "negative result"},
             }}) {
            result.push_back(value);
        }

        uint32_t state = 0xC001D00DU;
        for (size_t index = 0; index < 64; ++index) {
            state = state * 1664525U + 1013904223U;
            const uint32_t a = state;
            state = state * 1664525U + 1013904223U;
            const uint32_t b = state;
            result.push_back({a, b, static_cast<uint8_t>(index & 0x1FU),
                              "deterministic random " + std::to_string(index)});
        }
        return result;
    }();
    return values;
}

void checkALUComponent(const std::shared_ptr<IOComponent>& component,
                       const ALUCase& test_case,
                       const std::string& implementation) {
    const auto expected = expectedALU(test_case);
    const std::string prefix = implementation + " " + test_case.label + ": ";
    require(component->getOutputPin<32>("OUT")->getValueAsUInt64() == expected.out, prefix + "OUT");
    for (const auto& [pin, value] : std::array<std::pair<const char*, bool>, 7>{{
             {"ZERO", expected.zero}, {"EQ", expected.eq}, {"LT_SIGNED", expected.lt_signed},
             {"LT_UNSIGNED", expected.lt_unsigned}, {"NEGATIVE", expected.negative},
             {"CARRY_OUT", expected.carry}, {"OVERFLOW", expected.overflow}}}) {
        require(component->getOutputPin(pin)->getValue() == (value ? LogicValue::HIGH : LogicValue::LOW),
                prefix + pin);
    }
}

void verifyControlFlowImplementation(
    circuit::Fidelity fidelity,
    const std::string& implementation) {
    Simulator simulator;
    auto component = createFamilyDut(
        circuit::families::RV32IControlFlow, fidelity);
    ComponentBuilder builder(component);
    auto clk = builder.addNewWire("CLK_IN", nullptr, {component->getInputPin("CLK")});
    auto rst = builder.addNewWire("RST_IN", nullptr, {component->getInputPin("RST")});
    auto pc_write = builder.addNewWire("PC_WRITE_IN", nullptr, {component->getInputPin("PC_WRITE")});
    auto rs1 = builder.addNewWire<32>("RS1_IN", nullptr, {component->template getInputPin<32>("RS1_VALUE")});
    auto imm = builder.addNewWire<32>("IMM_IN", nullptr, {component->template getInputPin<32>("IMM")});
    auto branch = builder.addNewWire<3>("BRANCH_IN", nullptr, {component->template getInputPin<3>("BRANCH_TYPE")});
    auto jump = builder.addNewWire<2>("JUMP_IN", nullptr, {component->template getInputPin<2>("JUMP_TYPE")});
    auto eq = builder.addNewWire("EQ_IN", nullptr, {component->getInputPin("EQ")});
    auto lt_signed = builder.addNewWire("LT_SIGNED_IN", nullptr, {component->getInputPin("LT_SIGNED")});
    auto lt_unsigned = builder.addNewWire("LT_UNSIGNED_IN", nullptr, {component->getInputPin("LT_UNSIGNED")});
    for (const char* pin : {"PC", "PC_PLUS_4", "NEXT_PC_CANDIDATE"}) {
        builder.addNewWireDynamic(std::string("OUT_") + pin, 32,
                                  component->getOutputPinDynamic(pin), {});
    }
    for (const char* pin : {"BRANCH_TAKEN", "PC_MISALIGNED", "TARGET_MISALIGNED"}) {
        builder.addNewWire(std::string("OUT_") + pin, component->getOutputPin(pin), {});
    }

    driveBit(simulator, 0, clk, false);
    driveBit(simulator, 0, rst, true);
    driveBit(simulator, 0, pc_write, false);
    drive<32>(simulator, 0, rs1, 0);
    drive<32>(simulator, 0, imm, 0);
    drive<3>(simulator, 0, branch, 0);
    drive<2>(simulator, 0, jump, 0);
    driveBit(simulator, 0, eq, false);
    driveBit(simulator, 0, lt_signed, false);
    driveBit(simulator, 0, lt_unsigned, false);
    driveBit(simulator, 1000, rst, false);
    driveBit(simulator, 2000, pc_write, true);
    driveBit(simulator, 2200, clk, true);
    driveBit(simulator, 2500, clk, false);
    driveBit(simulator, 3000, pc_write, false);
    driveBit(simulator, 3200, clk, true);
    driveBit(simulator, 3500, clk, false);
    driveBit(simulator, 4000, pc_write, true);
    drive<32>(simulator, 4000, imm, 8);
    drive<3>(simulator, 4000, branch, 1);
    driveBit(simulator, 4000, eq, true);
    driveBit(simulator, 4200, clk, true);
    driveBit(simulator, 4500, clk, false);
    drive<32>(simulator, 5000, imm, 2);
    driveBit(simulator, 5000, eq, false);
    driveBit(simulator, 5200, clk, true);
    driveBit(simulator, 5500, clk, false);
    drive<32>(simulator, 6000, imm, 8);
    drive<3>(simulator, 6000, branch, 0);
    drive<2>(simulator, 6000, jump, 1);
    driveBit(simulator, 6200, clk, true);
    driveBit(simulator, 6500, clk, false);
    driveBit(simulator, 7000, pc_write, false);
    drive<2>(simulator, 7000, jump, 2);
    drive<32>(simulator, 7000, rs1, 0x101);
    drive<32>(simulator, 7000, imm, 2);
    drive<32>(simulator, 7500, rs1, 0x101);
    drive<32>(simulator, 7500, imm, 3);
    driveBit(simulator, 8000, pc_write, true);
    drive<32>(simulator, 8000, rs1, 0x101);
    drive<32>(simulator, 8000, imm, 2);
    driveBit(simulator, 8200, clk, true);
    driveBit(simulator, 8500, clk, false);
    driveBit(simulator, 9000, rst, true);
    driveBit(simulator, 9000, pc_write, false);
    drive<2>(simulator, 9000, jump, 0);
    drive<32>(simulator, 9000, imm, 0);
    driveBit(simulator, 9500, rst, false);
    drive<3>(simulator, 10000, branch, 2);
    driveBit(simulator, 10000, eq, false);
    drive<3>(simulator, 10500, branch, 3);
    driveBit(simulator, 10500, lt_signed, true);
    drive<3>(simulator, 11000, branch, 4);
    driveBit(simulator, 11000, lt_signed, false);
    drive<3>(simulator, 11500, branch, 5);
    driveBit(simulator, 11500, lt_unsigned, true);
    drive<3>(simulator, 12000, branch, 6);
    driveBit(simulator, 12000, lt_unsigned, false);
    size_t random_index = 0;
    for (const auto& test_case : controlFlowRandomCases()) {
        const size_t time = CONTROL_FLOW_RANDOM_START
                          + random_index * CONTROL_FLOW_RANDOM_STEP;
        drive<32>(simulator, time, rs1, test_case.rs1);
        drive<32>(simulator, time, imm, test_case.immediate);
        drive<3>(simulator, time, branch, test_case.branch_type);
        drive<2>(simulator, time, jump, test_case.jump_type);
        driveBit(simulator, time, eq, test_case.eq);
        driveBit(simulator, time, lt_signed, test_case.lt_signed);
        driveBit(simulator, time, lt_unsigned, test_case.lt_unsigned);
        ++random_index;
    }

    SimulationTest::scheduleInitialEventsForTree(component, simulator);
    simulator.runAndRecord(CONTROL_FLOW_RANDOM_START
                           + controlFlowRandomCases().size() * CONTROL_FLOW_RANDOM_STEP);
    for (const auto& expected : controlFlowExpected()) {
        simulator.setCircuitStateAtTime(expected.time);
        checkControlFlowComponent(component, expected, implementation);
    }
    random_index = 0;
    for (const auto& test_case : controlFlowRandomCases()) {
        const size_t sample_time = CONTROL_FLOW_RANDOM_START
                                 + random_index * CONTROL_FLOW_RANDOM_STEP
                                 + CONTROL_FLOW_RANDOM_STEP - 100;
        simulator.setCircuitStateAtTime(sample_time);
        checkControlFlowComponent(
            component, expectedRandomControlFlow(test_case, sample_time), implementation);
        ++random_index;
    }
}

void verifyDecodeImplementation(
    circuit::Fidelity fidelity,
    const std::string& implementation) {
    Simulator simulator;
    auto component = createFamilyDut(
        circuit::families::RV32IDecodeControl, fidelity);
    ComponentBuilder builder(component);
    auto instruction = builder.addNewWire<32>(
        "INSTRUCTION_IN", nullptr, {component->getInputPin<32>("INSTRUCTION")});
    for (const auto& [pin, width] : std::array<std::pair<const char*, size_t>, 12>{{
             {"RS1_ADDR", 5}, {"RS2_ADDR", 5}, {"RD_ADDR", 5}, {"IMM", 32},
             {"ALU_OP", 5}, {"ALU_A_SEL", 2}, {"ALU_B_SEL", 2}, {"WRITEBACK_SEL", 2},
             {"MEM_SIZE", 2}, {"BRANCH_TYPE", 3}, {"JUMP_TYPE", 2}, {"DECODE_TRAP_CAUSE", 4}}}) {
        builder.addNewWireDynamic(std::string("OUT_") + pin, width,
                                  component->getOutputPinDynamic(pin), {});
    }
    for (const char* pin : {"LEGAL", "REG_WRITE", "MEM_READ", "MEM_WRITE",
                            "LOAD_SIGN_EXTEND", "HALT_REQUEST", "TRAP_REQUEST"}) {
        builder.addNewWire(std::string("OUT_") + pin, component->getOutputPin(pin), {});
    }
    size_t index = 0;
    for (const auto& test_case : decodeCases()) {
        drive<32>(simulator, index * DECODE_STEP, instruction, test_case.raw);
        ++index;
    }
    SimulationTest::scheduleInitialEventsForTree(component, simulator);
    simulator.runAndRecord(decodeCases().size() * DECODE_STEP);
    index = 0;
    for (const auto& test_case : decodeCases()) {
        simulator.setCircuitStateAtTime(index * DECODE_STEP + DECODE_STEP - 100);
        checkDecodeComponent(component, test_case, implementation);
        ++index;
    }
}

void verifyRegisterFileImplementation(
    circuit::Fidelity fidelity,
    const std::string& implementation) {
    Simulator simulator;
    auto component = createFamilyDut(
        circuit::families::RegisterFile32x32, fidelity);
    ComponentBuilder builder(component);
    auto rs1_addr = builder.addNewWire<5>("RS1_ADDR_IN", nullptr,
        {component->getInputPin<5>("RS1_ADDR")});
    auto rs2_addr = builder.addNewWire<5>("RS2_ADDR_IN", nullptr,
        {component->getInputPin<5>("RS2_ADDR")});
    auto rd_addr = builder.addNewWire<5>("RD_ADDR_IN", nullptr,
        {component->getInputPin<5>("RD_ADDR")});
    auto write_data = builder.addNewWire<32>("WRITE_DATA_IN", nullptr,
        {component->getInputPin<32>("WRITE_DATA")});
    auto reg_write = builder.addNewWire("REG_WRITE_IN", nullptr,
        {component->getInputPin("REG_WRITE")});
    auto clk = builder.addNewWire("CLK_IN", nullptr, {component->getInputPin("CLK")});
    auto rst = builder.addNewWire("RST_IN", nullptr, {component->getInputPin("RST")});
    builder.addNewWire<32>("RS1_DATA_OUT", component->getOutputPin<32>("RS1_DATA"), {});
    builder.addNewWire<32>("RS2_DATA_OUT", component->getOutputPin<32>("RS2_DATA"), {});
    drive<5>(simulator, 0, rs1_addr, 0);
    drive<5>(simulator, 0, rs2_addr, 0);
    drive<5>(simulator, 0, rd_addr, 0);
    drive<32>(simulator, 0, write_data, 0);
    driveBit(simulator, 0, reg_write, false);
    driveBit(simulator, 0, clk, false);
    driveBit(simulator, 0, rst, true);
    driveBit(simulator, 1000, rst, false);
    for (size_t reg = 0; reg < 32; ++reg) {
        const size_t base = 2000 + reg * REGISTER_FILE_STEP;
        drive<5>(simulator, base, rd_addr, reg);
        drive<32>(simulator, base, write_data, registerPattern(reg));
        driveBit(simulator, base, reg_write, true);
        driveBit(simulator, base + 200, clk, true);
        driveBit(simulator, base + 400, clk, false);
        drive<5>(simulator, base + 500, rs1_addr, reg);
        drive<5>(simulator, base + 500, rs2_addr, reg == 0 ? 0 : reg - 1);
    }
    drive<5>(simulator, 34000, rd_addr, 5);
    drive<32>(simulator, 34000, write_data, 0xDEADBEEFU);
    driveBit(simulator, 34000, reg_write, false);
    driveBit(simulator, 34200, clk, true);
    driveBit(simulator, 34400, clk, false);
    drive<5>(simulator, 34500, rs1_addr, 5);
    drive<5>(simulator, 34500, rs2_addr, 31);
    driveBit(simulator, 35000, rst, true);
    drive<5>(simulator, 35500, rs1_addr, 5);
    drive<5>(simulator, 35500, rs2_addr, 31);
    SimulationTest::scheduleInitialEventsForTree(component, simulator);
    simulator.runAndRecord(36000);
    size_t row = 0;
    for (const auto& expected : registerFileExpected()) {
        simulator.setCircuitStateAtTime(expected.time);
        checkRegisterFileComponent(component, expected,
                                   implementation + " row " + std::to_string(row));
        ++row;
    }
}

void verifyALUImplementation(
    circuit::Fidelity fidelity,
    const std::string& implementation) {
    Simulator simulator;
    auto component = createFamilyDut(circuit::families::ALU32, fidelity);
    ComponentBuilder builder(component);
    auto a = builder.addNewWire<32>("A_IN", nullptr, {component->getInputPin<32>("A")});
    auto b = builder.addNewWire<32>("B_IN", nullptr, {component->getInputPin<32>("B")});
    auto op = builder.addNewWire<5>("OP_IN", nullptr, {component->getInputPin<5>("OP")});
    builder.addNewWire<32>("OUT", component->getOutputPin<32>("OUT"), {});
    for (const char* pin : {"ZERO", "EQ", "LT_SIGNED", "LT_UNSIGNED",
                            "NEGATIVE", "CARRY_OUT", "OVERFLOW"}) {
        builder.addNewWire(std::string("OUT_") + pin, component->getOutputPin(pin), {});
    }
    size_t row = 0;
    for (const auto& test_case : aluCases()) {
        const size_t time = row * ALU_STEP;
        drive<32>(simulator, time, a, test_case.a);
        drive<32>(simulator, time, b, test_case.b);
        drive<5>(simulator, time, op, test_case.op);
        ++row;
    }
    std::vector<LogicValue> partial_a(32, LogicValue::LOW);
    partial_a[7] = LogicValue::UNKNOWN;
    const size_t unknown_time = row * ALU_STEP;
    driveVector<32>(simulator, unknown_time, a, partial_a);
    drive<32>(simulator, unknown_time, b, 0x12345678U);
    drive<5>(simulator, unknown_time, op, ALU32Op::ADD);
    SimulationTest::scheduleInitialEventsForTree(component, simulator);
    simulator.runAndRecord((aluCases().size() + 1) * ALU_STEP);
    row = 0;
    for (const auto& test_case : aluCases()) {
        simulator.setCircuitStateAtTime(row * ALU_STEP + ALU_STEP - 100);
        checkALUComponent(component, test_case, implementation);
        ++row;
    }
    if (fidelity == circuit::Fidelity::Behavioral) {
        simulator.setCircuitStateAtTime(row * ALU_STEP + ALU_STEP - 100);
        require(component->getOutputPin<32>("OUT")->getValueAsVector()
                    == std::vector<LogicValue>(32, LogicValue::UNKNOWN),
                "behavioral ALU partial-unknown input must make OUT unknown");
        for (const char* pin : {"ZERO", "EQ", "LT_SIGNED", "LT_UNSIGNED",
                                "NEGATIVE", "CARRY_OUT", "OVERFLOW"}) {
            require(component->getOutputPin(pin)->getValue() == LogicValue::UNKNOWN,
                    std::string("behavioral ALU partial-unknown input must make ")
                        + pin + " unknown");
        }
    }
}
}

std::string RV32IControlFlowUnitEquivalenceTest::getTestName() const {
    return "RV32IControlFlowUnitEquivalenceTest";
}

void RV32IControlFlowUnitEquivalenceTest::verifyResults() {
    verifyControlFlowImplementation(circuit::Fidelity::Structural, "structural");
    verifyControlFlowImplementation(circuit::Fidelity::Behavioral, "behavioral");
}

std::string RV32IDecodeControlUnitEquivalenceTest::getTestName() const {
    return "RV32IDecodeControlUnitEquivalenceTest";
}

void RV32IDecodeControlUnitEquivalenceTest::verifyResults() {
    verifyDecodeImplementation(circuit::Fidelity::Structural, "structural");
    verifyDecodeImplementation(circuit::Fidelity::Behavioral, "behavioral");
}

std::string RV32IRegisterFileEquivalenceTest::getTestName() const {
    return "RV32IRegisterFileEquivalenceTest";
}

void RV32IRegisterFileEquivalenceTest::verifyResults() {
    verifyRegisterFileImplementation(circuit::Fidelity::Structural, "structural");
    verifyRegisterFileImplementation(circuit::Fidelity::Behavioral, "behavioral");
}

std::string ALU32EquivalenceTest::getTestName() const {
    return "ALU32EquivalenceTest";
}

void ALU32EquivalenceTest::verifyResults() {
    verifyALUImplementation(circuit::Fidelity::Structural, "structural");
    verifyALUImplementation(circuit::Fidelity::Behavioral, "behavioral");
}

namespace {
enum class StatusTerminal {
    None,
    Halt,
    Trap,
};

struct StatusInputs {
    bool enable = true;
    bool legal = true;
    bool reg_write = true;
    bool mem_read = false;
    bool mem_write = false;
    bool halt_request = false;
    bool trap_request = false;
    uint8_t decode_trap_cause = 0;
    uint32_t alu_address = 0;
    uint8_t mem_size = 2;
    bool pc_misaligned = false;
    bool target_misaligned = false;
    bool imem_ready = true;
    bool imem_fault = false;
    bool dmem_ready = true;
    bool dmem_fault = false;
};

struct StatusScenario {
    const char* label;
    StatusInputs input;
    bool pc_write;
    bool register_write;
    bool memory_request;
    bool data_misaligned;
    bool instruction_attempt;
    StatusTerminal terminal;
    uint8_t cause;
};

uint8_t statusCause(rv32i::RV32IExecutionTrapCause value) {
    return static_cast<uint8_t>(value);
}

const std::vector<StatusScenario>& statusScenarios() {
    static const std::vector<StatusScenario> values = [] {
        std::vector<StatusScenario> result;

        StatusInputs pc_priority;
        pc_priority.legal = false;
        pc_priority.mem_read = true;
        pc_priority.trap_request = true;
        pc_priority.decode_trap_cause = 2;
        pc_priority.halt_request = true;
        pc_priority.alu_address = 1;
        pc_priority.mem_size = 1;
        pc_priority.pc_misaligned = true;
        pc_priority.target_misaligned = true;
        pc_priority.imem_fault = true;
        pc_priority.dmem_fault = true;
        result.push_back({"PC misalignment has first priority", pc_priority,
                          false, false, false, true, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::InstructionAddressMisaligned)});

        StatusInputs imem_fault;
        imem_fault.imem_fault = true;
        result.push_back({"instruction access fault", imem_fault,
                          false, false, false, false, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::InstructionAccessFault)});

        StatusInputs illegal;
        illegal.legal = false;
        illegal.trap_request = true;
        illegal.decode_trap_cause = 2;
        result.push_back({"LEGAL low forces illegal-instruction cause", illegal,
                          false, false, false, false, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::IllegalInstruction)});

        StatusInputs ecall;
        ecall.trap_request = true;
        ecall.decode_trap_cause = 2;
        result.push_back({"ECALL decode trap", ecall,
                          false, false, false, false, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::EnvironmentCall)});

        StatusInputs halt;
        halt.halt_request = true;
        result.push_back({"EBREAK halt", halt,
                          false, false, false, false, true, StatusTerminal::Halt, 0});

        StatusInputs load_misaligned;
        load_misaligned.mem_read = true;
        load_misaligned.alu_address = 1;
        load_misaligned.mem_size = 1;
        result.push_back({"misaligned halfword load", load_misaligned,
                          false, false, true, true, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::LoadAddressMisaligned)});

        StatusInputs store_misaligned;
        store_misaligned.mem_write = true;
        store_misaligned.alu_address = 2;
        store_misaligned.mem_size = 2;
        result.push_back({"misaligned word store", store_misaligned,
                          false, false, true, true, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::StoreAddressMisaligned)});

        StatusInputs load_fault;
        load_fault.mem_read = true;
        load_fault.dmem_fault = true;
        result.push_back({"load access fault", load_fault,
                          false, false, true, false, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::LoadAccessFault)});

        StatusInputs store_fault;
        store_fault.mem_write = true;
        store_fault.dmem_fault = true;
        result.push_back({"store access fault", store_fault,
                          false, false, true, false, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::StoreAccessFault)});

        StatusInputs target;
        target.target_misaligned = true;
        result.push_back({"taken target misalignment", target,
                          false, false, false, false, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::InstructionAddressMisaligned)});

        StatusInputs data_priority;
        data_priority.mem_read = true;
        data_priority.alu_address = 1;
        data_priority.mem_size = 1;
        data_priority.dmem_fault = true;
        data_priority.target_misaligned = true;
        result.push_back({"data misalignment beats data fault and target fault", data_priority,
                          false, false, true, true, true, StatusTerminal::Trap,
                          statusCause(rv32i::RV32IExecutionTrapCause::LoadAddressMisaligned)});

        StatusInputs normal;
        result.push_back({"normal instruction commits", normal,
                          true, true, false, false, true, StatusTerminal::None, 0});
        return result;
    }();
    return values;
}

struct StatusExpected {
    size_t time;
    const char* label;
    bool pc_write;
    bool register_write;
    bool memory_request;
    bool halted;
    bool trapped;
    uint8_t cause;
    bool data_misaligned;
    bool instruction_attempt;
};

std::vector<StatusExpected> statusExpected() {
    std::vector<StatusExpected> result{
        {900, "reset clears state and permissions", false, false, false,
         false, false, 0, false, false},
        {1900, "normal non-memory instruction", true, true, false,
         false, false, 0, false, true},
        {2900, "disabled CPU waits", false, false, false,
         false, false, 0, false, false},
        {3900, "instruction memory wait", false, false, false,
         false, false, 0, false, false},
        {4900, "data memory wait keeps request active", false, false, true,
         false, false, 0, false, false},
        {5900, "ready load may commit", true, true, true,
         false, false, 0, false, true},
    };

    size_t base = 7000;
    for (const auto& scenario : statusScenarios()) {
        result.push_back({base + 900, scenario.label,
                          scenario.pc_write, scenario.register_write,
                          scenario.memory_request, false, false, 0,
                          scenario.data_misaligned, scenario.instruction_attempt});

        const bool halted = scenario.terminal == StatusTerminal::Halt;
        const bool trapped = scenario.terminal == StatusTerminal::Trap;
        result.push_back({base + 1900, scenario.label,
                          scenario.terminal == StatusTerminal::None ? scenario.pc_write : false,
                          scenario.terminal == StatusTerminal::None ? scenario.register_write : false,
                          scenario.terminal == StatusTerminal::None ? scenario.memory_request : false,
                          halted, trapped, scenario.cause,
                          scenario.data_misaligned,
                          scenario.terminal == StatusTerminal::None
                              ? scenario.instruction_attempt : false});
        base += 3000;
    }
    return result;
}

void checkStatusComponent(const std::shared_ptr<IOComponent>& component,
                          const StatusExpected& expected,
                          const std::string& implementation) {
    const std::string prefix = implementation + " " + expected.label
                             + " at " + std::to_string(expected.time) + ": ";
    for (const auto& [pin, value] : std::array<std::pair<const char*, bool>, 7>{{
             {"PC_WRITE", expected.pc_write},
             {"REGISTER_WRITE", expected.register_write},
             {"MEMORY_REQUEST_ACTIVE", expected.memory_request},
             {"HALTED", expected.halted},
             {"TRAPPED", expected.trapped},
             {"DATA_ADDRESS_MISALIGNED", expected.data_misaligned},
             {"INSTRUCTION_ATTEMPT", expected.instruction_attempt}}}) {
        require(component->getOutputPin(pin)->getValue()
                    == (value ? LogicValue::HIGH : LogicValue::LOW),
                prefix + pin);
    }
    require(component->getOutputPin<4>("TRAP_CAUSE")->getValueAsUInt64()
                == expected.cause,
            prefix + "TRAP_CAUSE");
}

void verifyStatusImplementation(
    circuit::Fidelity fidelity,
    const std::string& implementation) {
    Simulator simulator;
    auto component = createFamilyDut(
        circuit::families::RV32IExecutionStatus, fidelity);
    ComponentBuilder builder(component);
    auto bitInput = [&](const char* pin) {
        return builder.addNewWire(std::string(pin) + "_IN", nullptr,
                                  {component->getInputPin(pin)});
    };
    auto clk = bitInput("CLK");
    auto rst = bitInput("RST");
    auto enable = bitInput("ENABLE");
    auto legal = bitInput("LEGAL");
    auto reg_write = bitInput("REG_WRITE");
    auto mem_read = bitInput("MEM_READ");
    auto mem_write = bitInput("MEM_WRITE");
    auto halt_request = bitInput("HALT_REQUEST");
    auto trap_request = bitInput("TRAP_REQUEST");
    auto pc_misaligned = bitInput("PC_MISALIGNED");
    auto target_misaligned = bitInput("TARGET_MISALIGNED");
    auto imem_ready = bitInput("IMEM_READY");
    auto imem_fault = bitInput("IMEM_FAULT");
    auto dmem_ready = bitInput("DMEM_READY");
    auto dmem_fault = bitInput("DMEM_FAULT");
    auto decode_cause = builder.addNewWire<4>(
        "DECODE_TRAP_CAUSE_IN", nullptr,
        {component->getInputPin<4>("DECODE_TRAP_CAUSE")});
    auto address = builder.addNewWire<32>(
        "ALU_ADDRESS_IN", nullptr,
        {component->getInputPin<32>("ALU_ADDRESS")});
    auto mem_size = builder.addNewWire<2>(
        "MEM_SIZE_IN", nullptr,
        {component->getInputPin<2>("MEM_SIZE")});
    for (const char* pin : {"PC_WRITE", "REGISTER_WRITE", "MEMORY_REQUEST_ACTIVE",
                            "HALTED", "TRAPPED", "DATA_ADDRESS_MISALIGNED",
                            "INSTRUCTION_ATTEMPT"}) {
        builder.addNewWire(std::string("OUT_") + pin, component->getOutputPin(pin), {});
    }
    builder.addNewWire<4>("TRAP_CAUSE_OUT",
                          component->getOutputPin<4>("TRAP_CAUSE"), {});

    auto driveInputs = [&](size_t time, const StatusInputs& input) {
        driveBit(simulator, time, enable, input.enable);
        driveBit(simulator, time, legal, input.legal);
        driveBit(simulator, time, reg_write, input.reg_write);
        driveBit(simulator, time, mem_read, input.mem_read);
        driveBit(simulator, time, mem_write, input.mem_write);
        driveBit(simulator, time, halt_request, input.halt_request);
        driveBit(simulator, time, trap_request, input.trap_request);
        drive<4>(simulator, time, decode_cause, input.decode_trap_cause);
        drive<32>(simulator, time, address, input.alu_address);
        drive<2>(simulator, time, mem_size, input.mem_size);
        driveBit(simulator, time, pc_misaligned, input.pc_misaligned);
        driveBit(simulator, time, target_misaligned, input.target_misaligned);
        driveBit(simulator, time, imem_ready, input.imem_ready);
        driveBit(simulator, time, imem_fault, input.imem_fault);
        driveBit(simulator, time, dmem_ready, input.dmem_ready);
        driveBit(simulator, time, dmem_fault, input.dmem_fault);
    };

    StatusInputs normal;
    driveBit(simulator, 0, clk, false);
    driveBit(simulator, 0, rst, true);
    driveInputs(0, normal);
    driveBit(simulator, 1000, rst, false);
    StatusInputs disabled = normal;
    disabled.enable = false;
    driveInputs(2000, disabled);
    StatusInputs instruction_wait = normal;
    instruction_wait.imem_ready = false;
    driveInputs(3000, instruction_wait);
    StatusInputs data_wait = normal;
    data_wait.mem_read = true;
    data_wait.dmem_ready = false;
    driveInputs(4000, data_wait);
    StatusInputs data_ready = data_wait;
    data_ready.dmem_ready = true;
    driveInputs(5000, data_ready);
    size_t base = 7000;
    for (const auto& scenario : statusScenarios()) {
        driveBit(simulator, base, rst, true);
        driveBit(simulator, base, clk, false);
        driveInputs(base, normal);
        driveBit(simulator, base + 500, rst, false);
        driveInputs(base + 500, scenario.input);
        driveBit(simulator, base + 1100, clk, true);
        driveBit(simulator, base + 2100, clk, false);
        base += 3000;
    }
    SimulationTest::scheduleInitialEventsForTree(component, simulator);
    simulator.runAndRecord(7000 + statusScenarios().size() * 3000);
    for (const auto& expected : statusExpected()) {
        simulator.setCircuitStateAtTime(expected.time);
        checkStatusComponent(component, expected, implementation);
    }
}
}

std::string RV32IExecutionControlStatusUnitEquivalenceTest::getTestName() const {
    return "RV32IExecutionControlStatusUnitEquivalenceTest";
}

void RV32IExecutionControlStatusUnitEquivalenceTest::verifyResults() {
    verifyStatusImplementation(circuit::Fidelity::Structural, "structural");
    verifyStatusImplementation(circuit::Fidelity::Behavioral, "behavioral");
}
