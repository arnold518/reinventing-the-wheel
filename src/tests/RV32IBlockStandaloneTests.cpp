#include "tests/RV32IBlockStandaloneTests.hpp"

#include "components/selection/ComponentFamily.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
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
        checkpoint(
            "misaligned-jalr-candidate-held",
            {
                {"PC_WRITE", logicBit(false)},
                {"BRANCH_TYPE", logicBits(3, 0)},
                {"JUMP_TYPE", logicBits(2, 2)},
                {"RS1_VALUE", logicBits(32, 0x101)},
                {"IMM", logicBits(32, 2)},
            },
            controlFlowOutputs(
                12, 16, 0x102, false, false, true)),
        checkpoint(
            "reset-recovery",
            {
                {"RST", logicBit(true)},
                {"JUMP_TYPE", logicBits(2, 0)},
                {"IMM", logicBits(32, 0)},
            },
            controlFlowOutputs(0, 4, 4, false, false, false)),
    };

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
    const std::vector<std::pair<uint32_t, std::string>> cases{
        {encodeR(0x00, 7, 6, 0, 5), "add"},
        {encodeI(-12, 6, 2, 5, 0x03), "load-word"},
        {encodeS(-20, 7, 6, 2), "store-word"},
        {encodeB(-16, 7, 6, 0), "branch-equal"},
        {encodeJ(-2048, 5), "jump-and-link"},
        {0x00000073U, "environment-call"},
        {0x00100073U, "breakpoint"},
        {0x00000000U, "illegal-encoding"},
    };

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
    auto initial = normalStatusInputs();
    initial.emplace("CLK", logicBit(false));
    initial.emplace("RST", logicBit(true));

    auto reset_recovery = normalStatusInputs();
    reset_recovery.emplace("RST", logicBit(true));

    ActionScenario scenario{
        "contract",
        {},
        {},
        {100'000, 2'000'000},
    };
    scenario.actions = {
        checkpoint(
            "reset",
            std::move(initial),
            statusOutputs(
                false, false, false, false, false, 0, false, false)),
        checkpoint(
            "normal-completion",
            {{"RST", logicBit(false)}},
            statusOutputs(
                true, true, false, false, false, 0, false, true)),
        checkpoint(
            "data-memory-wait",
            {
                {"MEM_READ", logicBit(true)},
                {"DMEM_READY", logicBit(false)},
            },
            statusOutputs(
                false, false, true, false, false, 0, false, false),
            CheckpointKind::TransactionComplete),
        checkpoint(
            "misaligned-load-attempt",
            {
                {"DMEM_READY", logicBit(true)},
                {"ALU_ADDRESS", logicBits(32, 1)},
                {"MEM_SIZE", logicBits(2, 1)},
            },
            statusOutputs(
                false, false, true, false, false, 0, true, true),
            CheckpointKind::InstructionCommit),
        checkpoint(
            "load-trap-latched",
            {{"CLK", logicBit(true)}},
            statusOutputs(
                false,
                false,
                false,
                false,
                true,
                static_cast<uint8_t>(
                    rv32i::RV32IExecutionTrapCause::
                        LoadAddressMisaligned),
                true,
                false),
            CheckpointKind::AfterEdge),
        drive({{"CLK", logicBit(false)}}),
        checkpoint(
            "reset-recovery",
            std::move(reset_recovery),
            statusOutputs(
                false, false, false, false, false, 0, false, false)),
        checkpoint(
            "environment-call-attempt",
            {
                {"RST", logicBit(false)},
                {"TRAP_REQUEST", logicBit(true)},
                {"DECODE_TRAP_CAUSE", logicBits(4, 2)},
            },
            statusOutputs(
                false, false, false, false, false, 0, false, true),
            CheckpointKind::InstructionCommit),
        checkpoint(
            "environment-call-trap-latched",
            {{"CLK", logicBit(true)}},
            statusOutputs(
                false,
                false,
                false,
                false,
                true,
                static_cast<uint8_t>(
                    rv32i::RV32IExecutionTrapCause::EnvironmentCall),
                false,
                false),
            CheckpointKind::AfterEdge),
    };

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
    : ComponentScenarioTest(controlFlowSpec(), "contract") {}

RV32IDecodeControlUnitTest::RV32IDecodeControlUnitTest()
    : ComponentScenarioTest(
          decodeControlSpec(), "instruction-contract") {}

RV32IExecutionControlStatusUnitTest::
RV32IExecutionControlStatusUnitTest()
    : ComponentScenarioTest(executionStatusSpec(), "contract") {}
