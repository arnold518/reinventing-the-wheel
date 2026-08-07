#include "tests/RV32IBlockStandaloneTests.hpp"

#include "components/selection/ComponentFamily.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using circuit::test::ActionScenario;
using circuit::test::CheckpointKind;
using circuit::test::ComponentTestSpec;
using circuit::test::NamedValues;
using circuit::test::ScenarioAction;
using circuit::test::logicBit;
using circuit::test::logicBits;

ScenarioAction checkpoint(
    std::string id,
    NamedValues inputs,
    NamedValues outputs,
    CheckpointKind kind = CheckpointKind::Settled,
    std::string detail = {}) {
    return {
        std::move(id),
        kind,
        std::move(inputs),
        std::move(outputs),
        true,
        std::move(detail),
    };
}

ScenarioAction drive(NamedValues inputs) {
    ScenarioAction action;
    action.inputs = std::move(inputs);
    action.emit_checkpoint = false;
    return action;
}

NamedValues controlFlowOutputs(
    uint32_t pc,
    uint32_t plus4,
    uint32_t next,
    bool branch,
    bool pc_misaligned,
    bool target_misaligned) {
    return {
        {"PC", logicBits(32, pc)},
        {"PC_PLUS_4", logicBits(32, plus4)},
        {"NEXT_PC_CANDIDATE", logicBits(32, next)},
        {"BRANCH_TAKEN", logicBit(branch)},
        {"PC_MISALIGNED", logicBit(pc_misaligned)},
        {"TARGET_MISALIGNED", logicBit(target_misaligned)},
    };
}

ComponentTestSpec controlFlowSpec() {
    ActionScenario scenario{
        "contract",
        {},
        {},
        {100'000, 2'000'000},
    };
    scenario.actions = {
        checkpoint(
            "reset",
            {
                {"CLK", logicBit(false)},
                {"RST", logicBit(true)},
                {"PC_WRITE", logicBit(false)},
                {"RS1_VALUE", logicBits(32, 0)},
                {"IMM", logicBits(32, 0)},
                {"BRANCH_TYPE", logicBits(3, 0)},
                {"JUMP_TYPE", logicBits(2, 0)},
                {"EQ", logicBit(false)},
                {"LT_SIGNED", logicBit(false)},
                {"LT_UNSIGNED", logicBit(false)},
            },
            controlFlowOutputs(0, 4, 4, false, false, false)),
        checkpoint(
            "sequential-candidate",
            {{"RST", logicBit(false)}},
            controlFlowOutputs(0, 4, 4, false, false, false)),
        drive({{"PC_WRITE", logicBit(true)}}),
        checkpoint(
            "pc-plus-four-commit",
            {{"CLK", logicBit(true)}},
            controlFlowOutputs(4, 8, 8, false, false, false),
            CheckpointKind::AfterEdge),
        drive({{"CLK", logicBit(false)}}),
        drive({
            {"IMM", logicBits(32, 8)},
            {"BRANCH_TYPE", logicBits(3, 1)},
            {"EQ", logicBit(true)},
        }),
        checkpoint(
            "taken-branch-commit",
            {{"CLK", logicBit(true)}},
            controlFlowOutputs(12, 16, 20, true, false, false),
            CheckpointKind::AfterEdge),
        drive({{"CLK", logicBit(false)}}),
    };

    struct BranchCase {
        const char* id;
        uint8_t type;
        bool eq;
        bool lt_signed;
        bool lt_unsigned;
        bool taken;
    };
    const std::vector<BranchCase> branch_cases{
        {"no-branch", 0, false, false, false, false},
        {"beq-taken", 1, true, false, false, true},
        {"beq-not-taken", 1, false, false, false, false},
        {"bne-taken", 2, false, false, false, true},
        {"bne-not-taken", 2, true, false, false, false},
        {"blt-taken", 3, false, true, false, true},
        {"blt-not-taken", 3, false, false, false, false},
        {"bge-taken", 4, false, false, false, true},
        {"bge-not-taken", 4, false, true, false, false},
        {"bltu-taken", 5, false, false, true, true},
        {"bltu-not-taken", 5, false, false, false, false},
        {"bgeu-taken", 6, false, false, false, true},
        {"bgeu-not-taken", 6, false, false, true, false},
        {"reserved-branch", 7, true, true, true, false},
    };
    for (const auto& branch_case : branch_cases) {
        scenario.actions.push_back(checkpoint(
            branch_case.id,
            {
                {"PC_WRITE", logicBit(false)},
                {"IMM", logicBits(32, 8)},
                {"BRANCH_TYPE", logicBits(3, branch_case.type)},
                {"JUMP_TYPE", logicBits(2, 0)},
                {"EQ", logicBit(branch_case.eq)},
                {"LT_SIGNED", logicBit(branch_case.lt_signed)},
                {"LT_UNSIGNED", logicBit(branch_case.lt_unsigned)},
            },
            controlFlowOutputs(
                12,
                16,
                branch_case.taken ? 20 : 16,
                branch_case.taken,
                false,
                false)));
    }

    scenario.actions.push_back(checkpoint(
        "jal-candidate",
        {
            {"BRANCH_TYPE", logicBits(3, 0)},
            {"JUMP_TYPE", logicBits(2, 1)},
            {"IMM", logicBits(32, 8)},
        },
        controlFlowOutputs(12, 16, 20, false, false, false)));
    scenario.actions.push_back(checkpoint(
        "pc-write-disabled-holds",
        {{"CLK", logicBit(true)}},
        controlFlowOutputs(12, 16, 20, false, false, false),
        CheckpointKind::AfterEdge));
    scenario.actions.push_back(drive({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpoint(
        "misaligned-jal",
        {{"IMM", logicBits(32, 2)}},
        controlFlowOutputs(12, 16, 14, false, false, true)));
    scenario.actions.push_back(checkpoint(
        "misaligned-jalr-masks-bit-zero",
        {
            {"JUMP_TYPE", logicBits(2, 2)},
            {"RS1_VALUE", logicBits(32, 0x101)},
            {"IMM", logicBits(32, 2)},
        },
        controlFlowOutputs(12, 16, 0x102, false, false, true)));
    scenario.actions.push_back(checkpoint(
        "reserved-jump-selects-zero",
        {{"JUMP_TYPE", logicBits(2, 3)}},
        controlFlowOutputs(12, 16, 0, false, false, false)));
    scenario.actions.push_back(checkpoint(
        "misaligned-taken-branch",
        {
            {"BRANCH_TYPE", logicBits(3, 1)},
            {"JUMP_TYPE", logicBits(2, 0)},
            {"EQ", logicBit(true)},
            {"IMM", logicBits(32, 2)},
        },
        controlFlowOutputs(12, 16, 14, true, false, true)));

    scenario.actions.push_back(checkpoint(
        "misaligned-pc-before-commit",
        {
            {"PC_WRITE", logicBit(true)},
            {"BRANCH_TYPE", logicBits(3, 0)},
            {"JUMP_TYPE", logicBits(2, 2)},
            {"RS1_VALUE", logicBits(32, 0x103)},
            {"IMM", logicBits(32, 0)},
        },
        controlFlowOutputs(
            12, 16, 0x102, false, false, true)));
    scenario.actions.push_back(checkpoint(
        "commit-misaligned-pc",
        {{"CLK", logicBit(true)}},
        controlFlowOutputs(
            0x102, 0x106, 0x102, false, true, true),
        CheckpointKind::AfterEdge));
    scenario.actions.push_back(drive({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpoint(
        "reset-after-misaligned-pc",
        {
            {"RST", logicBit(true)},
            {"PC_WRITE", logicBit(false)},
            {"JUMP_TYPE", logicBits(2, 0)},
            {"IMM", logicBits(32, 0)},
        },
        controlFlowOutputs(0, 4, 4, false, false, false)));

    scenario.actions.push_back(drive({{"RST", logicBit(false)}}));
    scenario.actions.push_back(checkpoint(
        "last-aligned-word-before-commit",
        {
            {"PC_WRITE", logicBit(true)},
            {"JUMP_TYPE", logicBits(2, 2)},
            {"RS1_VALUE", logicBits(32, 0xfffffffcU)},
        },
        controlFlowOutputs(
            0, 4, 0xfffffffcU, false, false, false)));
    scenario.actions.push_back(checkpoint(
        "commit-last-aligned-word",
        {{"CLK", logicBit(true)}},
        controlFlowOutputs(
            0xfffffffcU,
            0,
            0xfffffffcU,
            false,
            false,
            false),
        CheckpointKind::AfterEdge));
    scenario.actions.push_back(drive({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpoint(
        "pc-plus-four-wraps",
        {
            {"JUMP_TYPE", logicBits(2, 0)},
            {"PC_WRITE", logicBit(false)},
        },
        controlFlowOutputs(
            0xfffffffcU, 0, 0, false, false, false)));
    scenario.actions.push_back(checkpoint(
        "final-reset-recovery",
        {{"RST", logicBit(true)}},
        controlFlowOutputs(0, 4, 4, false, false, false)));

    return {
        "RV32IControlFlowUnitTest",
        std::string(circuit::families::RV32IControlFlow.id()),
        "RV32I_CONTROL_FLOW_ROOT",
        {},
        {std::move(scenario)},
    };
}

uint32_t encodeR(
    uint8_t funct7,
    uint8_t rs2,
    uint8_t rs1,
    uint8_t funct3,
    uint8_t rd) {
    return (static_cast<uint32_t>(funct7) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | 0x33U;
}

uint32_t encodeI(
    int32_t immediate,
    uint8_t rs1,
    uint8_t funct3,
    uint8_t rd,
    uint8_t opcode) {
    return ((static_cast<uint32_t>(immediate) & 0xFFFU) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | opcode;
}

uint32_t encodeShiftI(
    uint8_t funct7,
    uint8_t shamt,
    uint8_t rs1,
    uint8_t funct3,
    uint8_t rd) {
    return (static_cast<uint32_t>(funct7) << 25U)
         | (static_cast<uint32_t>(shamt & 0x1FU) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | 0x13U;
}

uint32_t encodeS(
    int32_t immediate,
    uint8_t rs2,
    uint8_t rs1,
    uint8_t funct3) {
    const auto bits = static_cast<uint32_t>(immediate) & 0xFFFU;
    return ((bits >> 5U) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | ((bits & 0x1FU) << 7U)
         | 0x23U;
}

uint32_t encodeB(
    int32_t immediate,
    uint8_t rs2,
    uint8_t rs1,
    uint8_t funct3) {
    const auto bits = static_cast<uint32_t>(immediate) & 0x1FFFU;
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
    return (immediate & 0xFFFFF000U)
         | (static_cast<uint32_t>(rd) << 7U)
         | opcode;
}

uint32_t encodeJ(int32_t immediate, uint8_t rd) {
    const auto bits = static_cast<uint32_t>(immediate) & 0x1FFFFFU;
    return (((bits >> 20U) & 1U) << 31U)
         | (((bits >> 1U) & 0x3FFU) << 21U)
         | (((bits >> 11U) & 1U) << 20U)
         | (((bits >> 12U) & 0xFFU) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | 0x6FU;
}

NamedValues decodeOutputs(uint32_t instruction) {
    const auto decoded = rv32i::RV32IDecoder::decode(instruction);
    const auto control = rv32i::RV32IControl::fromDecoded(decoded);
    return {
        {"RS1_ADDR", logicBits(5, decoded.rs1)},
        {"RS2_ADDR", logicBits(5, decoded.rs2)},
        {"RD_ADDR", logicBits(5, decoded.rd)},
        {"IMM", logicBits(
            32, static_cast<uint32_t>(decoded.immediate))},
        {"ALU_OP", logicBits(5, control.alu_op)},
        {"ALU_A_SEL", logicBits(
            2, rv32i::component_encoding::aluSourceA(control.alu_a))},
        {"ALU_B_SEL", logicBits(
            2, rv32i::component_encoding::aluSourceB(control.alu_b))},
        {"LEGAL", logicBit(control.legal)},
        {"REG_WRITE", logicBit(control.reg_write)},
        {"MEM_READ", logicBit(control.mem_read)},
        {"MEM_WRITE", logicBit(control.mem_write)},
        {"WRITEBACK_SEL", logicBits(
            2,
            rv32i::component_encoding::writeback(control.writeback))},
        {"MEM_SIZE", logicBits(
            2, rv32i::component_encoding::memorySize(control.mem_size))},
        {"LOAD_SIGN_EXTEND", logicBit(control.load_sign_extend)},
        {"USES_RS1", logicBit(control.uses_rs1)},
        {"USES_RS2", logicBit(control.uses_rs2)},
        {"BRANCH_TYPE", logicBits(
            3, rv32i::component_encoding::branch(control.branch))},
        {"JUMP_TYPE", logicBits(
            2, rv32i::component_encoding::jump(control.jump))},
        {"HALT_REQUEST", logicBit(control.halt)},
        {"TRAP_REQUEST", logicBit(control.trap)},
        {"DECODE_TRAP_CAUSE", logicBits(
            4,
            rv32i::component_encoding::decodeTrapCause(
                control.trap_cause))},
    };
}

ComponentTestSpec decodeControlSpec() {
    const std::vector<std::pair<uint32_t, std::string>> legal_cases{
        {encodeR(0x00, 12, 11, 0x0, 10), "add"},
        {encodeR(0x20, 12, 11, 0x0, 10), "subtract"},
        {encodeR(0x00, 12, 11, 0x1, 10), "shift-left-logical"},
        {encodeR(0x00, 12, 11, 0x2, 10), "set-less-than"},
        {encodeR(0x00, 12, 11, 0x3, 10), "set-less-than-unsigned"},
        {encodeR(0x00, 12, 11, 0x4, 10), "exclusive-or"},
        {encodeR(0x00, 12, 11, 0x5, 10), "shift-right-logical"},
        {encodeR(0x20, 12, 11, 0x5, 10), "shift-right-arithmetic"},
        {encodeR(0x00, 12, 11, 0x6, 10), "or"},
        {encodeR(0x00, 12, 11, 0x7, 10), "and"},

        {encodeI(-1, 5, 0x0, 6, 0x13), "add-immediate"},
        {encodeI(-2048, 5, 0x2, 6, 0x13), "set-less-than-immediate"},
        {encodeI(2047, 5, 0x3, 6, 0x13), "set-less-than-immediate-unsigned"},
        {encodeI(0x55, 5, 0x4, 6, 0x13), "exclusive-or-immediate"},
        {encodeI(0x123, 5, 0x6, 6, 0x13), "or-immediate"},
        {encodeI(0x321, 5, 0x7, 6, 0x13), "and-immediate"},
        {encodeShiftI(0x00, 5, 5, 0x1, 6), "shift-left-logical-immediate"},
        {encodeShiftI(0x00, 6, 5, 0x5, 6), "shift-right-logical-immediate"},
        {encodeShiftI(0x20, 31, 5, 0x5, 6), "shift-right-arithmetic-immediate"},

        {encodeI(-16, 3, 0x0, 4, 0x03), "load-byte"},
        {encodeI(18, 3, 0x1, 4, 0x03), "load-halfword"},
        {encodeI(20, 3, 0x2, 4, 0x03), "load-word"},
        {encodeI(21, 3, 0x4, 4, 0x03), "load-byte-unsigned"},
        {encodeI(22, 3, 0x5, 4, 0x03), "load-halfword-unsigned"},

        {encodeS(-20, 8, 7, 0x0), "store-byte"},
        {encodeS(24, 8, 7, 0x1), "store-halfword"},
        {encodeS(28, 8, 7, 0x2), "store-word"},

        {encodeB(16, 2, 1, 0x0), "branch-equal"},
        {encodeB(-4, 2, 1, 0x1), "branch-not-equal"},
        {encodeB(32, 2, 1, 0x4), "branch-less-than"},
        {encodeB(-32, 2, 1, 0x5), "branch-greater-or-equal"},
        {encodeB(64, 2, 1, 0x6), "branch-less-than-unsigned"},
        {encodeB(-64, 2, 1, 0x7), "branch-greater-or-equal-unsigned"},

        {encodeJ(-1'048'576, 5), "jump-and-link"},
        {encodeI(-12, 5, 0x0, 1, 0x67), "jump-and-link-register"},
        {encodeU(0x12345000U, 9, 0x37), "load-upper-immediate"},
        {encodeU(0xFFFFF000U, 9, 0x17), "add-upper-immediate-to-pc"},
        {encodeI(0x0FF, 0, 0x0, 0, 0x0F), "fence"},
        {0x00000073U, "environment-call"},
        {0x00100073U, "breakpoint"},
    };

    const std::vector<std::pair<uint32_t, std::string>> illegal_cases{
        {0x00000000U, "illegal-instruction-length"},
        {0x0000007BU, "illegal-unknown-opcode"},
        {encodeI(0, 1, 0x0, 2, 0x1B), "illegal-rv64-op-immediate"},
        {encodeR(0x01, 2, 1, 0x0, 3), "illegal-register-funct7"},
        {encodeShiftI(0x01, 5, 1, 0x5, 2), "illegal-shift-funct7"},
        {encodeB(4, 2, 1, 0x2), "illegal-branch-funct3"},
        {encodeI(0, 1, 0x3, 2, 0x03), "illegal-load-funct3"},
        {encodeS(0, 2, 1, 0x3), "illegal-store-funct3"},
        {encodeI(0, 1, 0x1, 2, 0x67), "illegal-jalr-funct3"},
        {encodeI(0, 0, 0x1, 0, 0x0F), "illegal-fence-i"},
        {0x00101073U, "illegal-csr"},
        {0x00200073U, "illegal-system-operation"},
    };

    if (legal_cases.size() != 40 || illegal_cases.size() != 12) {
        throw std::logic_error(
            "RV32I decode/control contract vector counts changed");
    }

    std::vector<std::pair<uint32_t, std::string>> cases;
    cases.reserve(legal_cases.size() + illegal_cases.size() + 64);
    cases.insert(cases.end(), legal_cases.begin(), legal_cases.end());
    cases.insert(cases.end(), illegal_cases.begin(), illegal_cases.end());

    uint32_t random_state = 0xC001D00DU;
    for (size_t index = 0; index < 64; ++index) {
        random_state = random_state * 1'664'525U + 1'013'904'223U;
        cases.emplace_back(
            random_state,
            "deterministic-raw-" + std::to_string(index));
    }

    ActionScenario scenario{
        "instruction-contract",
        {},
        {},
        {500'000, 4'000'000},
    };
    for (const auto& [instruction, id] : cases) {
        scenario.actions.push_back(checkpoint(
            id,
            {{"INSTRUCTION", logicBits(32, instruction)}},
            decodeOutputs(instruction)));
    }

    return {
        "RV32IDecodeControlUnitTest",
        std::string(circuit::families::RV32IDecodeControl.id()),
        "RV32I_DECODE_CONTROL_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues statusOutputs(
    bool pc_write,
    bool register_write,
    bool request,
    bool halted,
    bool trapped,
    uint8_t cause,
    bool misaligned,
    bool attempt) {
    return {
        {"PC_WRITE", logicBit(pc_write)},
        {"REGISTER_WRITE", logicBit(register_write)},
        {"MEMORY_REQUEST_ACTIVE", logicBit(request)},
        {"HALTED", logicBit(halted)},
        {"TRAPPED", logicBit(trapped)},
        {"TRAP_CAUSE", logicBits(4, cause)},
        {"DATA_ADDRESS_MISALIGNED", logicBit(misaligned)},
        {"INSTRUCTION_ATTEMPT", logicBit(attempt)},
    };
}

NamedValues normalStatusInputs() {
    return {
        {"ENABLE", logicBit(true)},
        {"LEGAL", logicBit(true)},
        {"REG_WRITE", logicBit(true)},
        {"MEM_READ", logicBit(false)},
        {"MEM_WRITE", logicBit(false)},
        {"HALT_REQUEST", logicBit(false)},
        {"TRAP_REQUEST", logicBit(false)},
        {"DECODE_TRAP_CAUSE", logicBits(4, 0)},
        {"ALU_ADDRESS", logicBits(32, 0)},
        {"MEM_SIZE", logicBits(2, 2)},
        {"PC_MISALIGNED", logicBit(false)},
        {"TARGET_MISALIGNED", logicBit(false)},
        {"IMEM_READY", logicBit(true)},
        {"IMEM_FAULT", logicBit(false)},
        {"DMEM_READY", logicBit(true)},
        {"DMEM_FAULT", logicBit(false)},
    };
}

ComponentTestSpec executionStatusSpec() {
    ActionScenario scenario{
        "contract",
        {},
        {},
        {250'000, 4'000'000},
    };
    auto initial = normalStatusInputs();
    initial.emplace("CLK", logicBit(false));
    initial.emplace("RST", logicBit(true));
    scenario.actions.push_back(checkpoint(
        "reset",
        std::move(initial),
        statusOutputs(
            false, false, false, false, false, 0, false, false)));

    auto normal = normalStatusInputs();
    normal.emplace("CLK", logicBit(false));
    normal.emplace("RST", logicBit(false));
    scenario.actions.push_back(checkpoint(
        "normal-completion",
        std::move(normal),
        statusOutputs(
            true, true, false, false, false, 0, false, true)));

    auto disabled = normalStatusInputs();
    disabled["ENABLE"] = logicBit(false);
    scenario.actions.push_back(checkpoint(
        "disabled",
        std::move(disabled),
        statusOutputs(
            false, false, false, false, false, 0, false, false)));

    auto instruction_wait = normalStatusInputs();
    instruction_wait["IMEM_READY"] = logicBit(false);
    scenario.actions.push_back(checkpoint(
        "instruction-memory-wait",
        std::move(instruction_wait),
        statusOutputs(
            false, false, false, false, false, 0, false, false),
        CheckpointKind::TransactionComplete));

    auto load_wait = normalStatusInputs();
    load_wait["MEM_READ"] = logicBit(true);
    load_wait["DMEM_READY"] = logicBit(false);
    scenario.actions.push_back(checkpoint(
        "data-memory-wait",
        std::move(load_wait),
        statusOutputs(
            false, false, true, false, false, 0, false, false),
        CheckpointKind::TransactionComplete));

    auto load_ready = normalStatusInputs();
    load_ready["MEM_READ"] = logicBit(true);
    scenario.actions.push_back(checkpoint(
        "load-completion",
        std::move(load_ready),
        statusOutputs(
            true, true, true, false, false, 0, false, true),
        CheckpointKind::TransactionComplete));

    auto store_ready = normalStatusInputs();
    store_ready["REG_WRITE"] = logicBit(false);
    store_ready["MEM_WRITE"] = logicBit(true);
    scenario.actions.push_back(checkpoint(
        "store-completion",
        std::move(store_ready),
        statusOutputs(
            true, false, true, false, false, 0, false, true),
        CheckpointKind::TransactionComplete));

    for (uint8_t size = 0; size < 4; ++size) {
        for (uint32_t address_low = 0; address_low < 4; ++address_low) {
            const bool misaligned =
                size == 3
                || (size == 1 && (address_low & 1U) != 0)
                || (size == 2 && address_low != 0);
            auto alignment_inputs = normalStatusInputs();
            alignment_inputs["ENABLE"] = logicBit(false);
            alignment_inputs["ALU_ADDRESS"] =
                logicBits(32, address_low);
            alignment_inputs["MEM_SIZE"] = logicBits(2, size);
            scenario.actions.push_back(checkpoint(
                "alignment-size-" + std::to_string(size)
                    + "-address-" + std::to_string(address_low),
                std::move(alignment_inputs),
                statusOutputs(
                    false,
                    false,
                    false,
                    false,
                    false,
                    0,
                    misaligned,
                    false)));
        }
    }

    const auto encodedCause = [](rv32i::RV32IExecutionTrapCause cause) {
        return rv32i::component_encoding::executionTrapCause(cause);
    };
    const auto appendEvent = [&](
        const std::string& id,
        NamedValues overrides,
        rv32i::RV32IExecutionTrapCause cause,
        bool halt,
        bool memory_request,
        bool data_misaligned) {
        auto inputs = normalStatusInputs();
        inputs.emplace("CLK", logicBit(false));
        inputs.emplace("RST", logicBit(false));
        for (auto& [name, value] : overrides) {
            inputs[name] = std::move(value);
        }

        scenario.actions.push_back(checkpoint(
            id + "-attempt",
            std::move(inputs),
            statusOutputs(
                false,
                false,
                memory_request,
                false,
                false,
                0,
                data_misaligned,
                true),
            CheckpointKind::InstructionCommit));
        scenario.actions.push_back(checkpoint(
            id + "-latched",
            {{"CLK", logicBit(true)}},
            statusOutputs(
                false,
                false,
                false,
                halt,
                !halt,
                halt ? 0 : encodedCause(cause),
                data_misaligned,
                false),
            CheckpointKind::AfterEdge));
        scenario.actions.push_back(checkpoint(
            id + "-sticky",
            {
                {"CLK", logicBit(false)},
                {"HALT_REQUEST", logicBit(false)},
                {"TRAP_REQUEST", logicBit(false)},
                {"LEGAL", logicBit(true)},
                {"PC_MISALIGNED", logicBit(false)},
                {"TARGET_MISALIGNED", logicBit(false)},
                {"IMEM_FAULT", logicBit(false)},
                {"DMEM_FAULT", logicBit(false)},
                {"MEM_READ", logicBit(false)},
                {"MEM_WRITE", logicBit(false)},
                {"ALU_ADDRESS", logicBits(32, 0)},
                {"MEM_SIZE", logicBits(2, 2)},
            },
            statusOutputs(
                false,
                false,
                false,
                halt,
                !halt,
                halt ? 0 : encodedCause(cause),
                false,
                false)));

        auto reset = normalStatusInputs();
        reset.emplace("CLK", logicBit(false));
        reset.emplace("RST", logicBit(true));
        scenario.actions.push_back(checkpoint(
            id + "-reset",
            std::move(reset),
            statusOutputs(
                false,
                false,
                false,
                false,
                false,
                0,
                false,
                false)));
    };

    appendEvent(
        "pc-misaligned",
        {{"PC_MISALIGNED", logicBit(true)}},
        rv32i::RV32IExecutionTrapCause::
            InstructionAddressMisaligned,
        false,
        false,
        false);
    appendEvent(
        "instruction-access-fault",
        {{"IMEM_FAULT", logicBit(true)}},
        rv32i::RV32IExecutionTrapCause::InstructionAccessFault,
        false,
        false,
        false);
    appendEvent(
        "illegal-instruction",
        {{"LEGAL", logicBit(false)}},
        rv32i::RV32IExecutionTrapCause::IllegalInstruction,
        false,
        false,
        false);
    appendEvent(
        "environment-call",
        {
            {"TRAP_REQUEST", logicBit(true)},
            {"DECODE_TRAP_CAUSE", logicBits(
                4,
                encodedCause(
                    rv32i::RV32IExecutionTrapCause::
                        EnvironmentCall))},
        },
        rv32i::RV32IExecutionTrapCause::EnvironmentCall,
        false,
        false,
        false);
    appendEvent(
        "halt",
        {{"HALT_REQUEST", logicBit(true)}},
        rv32i::RV32IExecutionTrapCause::None,
        true,
        false,
        false);
    appendEvent(
        "load-address-misaligned",
        {
            {"MEM_READ", logicBit(true)},
            {"ALU_ADDRESS", logicBits(32, 1)},
            {"MEM_SIZE", logicBits(2, 1)},
        },
        rv32i::RV32IExecutionTrapCause::LoadAddressMisaligned,
        false,
        true,
        true);
    appendEvent(
        "store-address-misaligned",
        {
            {"REG_WRITE", logicBit(false)},
            {"MEM_WRITE", logicBit(true)},
            {"ALU_ADDRESS", logicBits(32, 2)},
            {"MEM_SIZE", logicBits(2, 2)},
        },
        rv32i::RV32IExecutionTrapCause::StoreAddressMisaligned,
        false,
        true,
        true);
    appendEvent(
        "load-access-fault",
        {
            {"MEM_READ", logicBit(true)},
            {"DMEM_FAULT", logicBit(true)},
        },
        rv32i::RV32IExecutionTrapCause::LoadAccessFault,
        false,
        true,
        false);
    appendEvent(
        "store-access-fault",
        {
            {"REG_WRITE", logicBit(false)},
            {"MEM_WRITE", logicBit(true)},
            {"DMEM_FAULT", logicBit(true)},
        },
        rv32i::RV32IExecutionTrapCause::StoreAccessFault,
        false,
        true,
        false);
    appendEvent(
        "target-misaligned",
        {{"TARGET_MISALIGNED", logicBit(true)}},
        rv32i::RV32IExecutionTrapCause::
            InstructionAddressMisaligned,
        false,
        false,
        false);
    appendEvent(
        "priority-pc-before-other-faults",
        {
            {"PC_MISALIGNED", logicBit(true)},
            {"IMEM_FAULT", logicBit(true)},
            {"LEGAL", logicBit(false)},
            {"HALT_REQUEST", logicBit(true)},
            {"TARGET_MISALIGNED", logicBit(true)},
        },
        rv32i::RV32IExecutionTrapCause::
            InstructionAddressMisaligned,
        false,
        false,
        false);

    auto stalled_fault = normalStatusInputs();
    stalled_fault.emplace("CLK", logicBit(false));
    stalled_fault.emplace("RST", logicBit(false));
    stalled_fault["MEM_READ"] = logicBit(true);
    stalled_fault["DMEM_READY"] = logicBit(false);
    stalled_fault["DMEM_FAULT"] = logicBit(true);
    scenario.actions.push_back(checkpoint(
        "data-fault-waits-for-response",
        std::move(stalled_fault),
        statusOutputs(
            false, false, true, false, false, 0, false, false),
        CheckpointKind::TransactionComplete));

    auto fetch_stalled_fault = normalStatusInputs();
    fetch_stalled_fault.emplace("CLK", logicBit(false));
    fetch_stalled_fault.emplace("RST", logicBit(false));
    fetch_stalled_fault["IMEM_READY"] = logicBit(false);
    fetch_stalled_fault["IMEM_FAULT"] = logicBit(true);
    scenario.actions.push_back(checkpoint(
        "instruction-fault-waits-for-response",
        std::move(fetch_stalled_fault),
        statusOutputs(
            false, false, false, false, false, 0, false, false),
        CheckpointKind::TransactionComplete));

    return {
        "RV32IExecutionControlStatusUnitTest",
        std::string(circuit::families::RV32IExecutionStatus.id()),
        "RV32I_EXECUTION_CONTROL_STATUS_ROOT",
        {},
        {std::move(scenario)},
    };
}
}

RV32IControlFlowUnitTest::RV32IControlFlowUnitTest()
    : ComponentScenarioTest(
          controlFlowSpec(),
          "contract",
          rv32i::withBehavioralMemoryParts(
              circuit::canonicalDefaultProfile())) {}

RV32IDecodeControlUnitTest::RV32IDecodeControlUnitTest()
    : ComponentScenarioTest(
          decodeControlSpec(),
          "instruction-contract",
          rv32i::withBehavioralMemoryParts(
              circuit::canonicalDefaultProfile())) {}

RV32IExecutionControlStatusUnitTest::
RV32IExecutionControlStatusUnitTest()
    : ComponentScenarioTest(
          executionStatusSpec(),
          "contract",
          rv32i::withBehavioralMemoryParts(
              circuit::canonicalDefaultProfile())) {}
