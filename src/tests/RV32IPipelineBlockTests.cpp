#include "tests/RV32IPipelineBlockTests.hpp"

#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "tests/TestHelpers.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
using circuit::ComponentFamily;
using circuit::test::ActionScenario;
using circuit::test::CheckpointKind;
using circuit::test::ComponentTestSpec;
using circuit::test::NamedValues;
using circuit::test::ScenarioAction;
using circuit::test::logicBit;
using circuit::test::logicBits;

circuit::BuildProfile rv32iPreviewProfile() {
    return rv32i::withBehavioralMemoryParts(
        circuit::canonicalDefaultProfile(),
        "rv32i-component-preview");
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ScenarioAction checkpoint(
    std::string id,
    NamedValues inputs,
    NamedValues outputs,
    CheckpointKind kind = CheckpointKind::Settled) {
    return {
        std::move(id),
        kind,
        std::move(inputs),
        std::move(outputs),
        true,
        {},
    };
}

ScenarioAction drive(NamedValues inputs) {
    ScenarioAction action;
    action.inputs = std::move(inputs);
    action.emit_checkpoint = false;
    return action;
}

NamedValues registerInputs(
    const std::vector<std::string>& fields,
    bool clk,
    bool rst,
    bool write_enable,
    bool flush,
    bool valid,
    uint32_t seed) {
    NamedValues inputs{
        {"CLK", logicBit(clk)},
        {"RST", logicBit(rst)},
        {"WRITE_ENABLE", logicBit(write_enable)},
        {"FLUSH", logicBit(flush)},
        {"D_VALID", logicBit(valid)},
    };
    for (size_t index = 0; index < fields.size(); ++index) {
        inputs.emplace(
            "D_" + fields[index],
            logicBits(32, seed + static_cast<uint32_t>(index)));
    }
    return inputs;
}

NamedValues registerOutputs(
    const std::vector<std::string>& fields,
    bool valid,
    uint32_t seed) {
    NamedValues outputs{{"Q_VALID", logicBit(valid)}};
    for (size_t index = 0; index < fields.size(); ++index) {
        const auto value = seed == 0
            ? uint32_t{0}
            : seed + static_cast<uint32_t>(index);
        outputs.emplace(
            "Q_" + fields[index],
            logicBits(32, value));
    }
    return outputs;
}

ComponentTestSpec pipelineRegisterSpec(
    std::string test_id,
    const ComponentFamily& family,
    std::vector<std::string> fields) {
    ActionScenario scenario{
        "contract",
        {},
        {},
        {200'000, 4'000'000},
    };
    scenario.actions = {
        checkpoint(
            "reset",
            registerInputs(
                fields, false, true, false, false, false, 0x10),
            registerOutputs(fields, false, 0)),
        checkpoint(
            "prepare-first-payload",
            registerInputs(
                fields, false, false, true, false, true, 0x100),
            registerOutputs(fields, false, 0)),
        checkpoint(
            "capture-first-payload",
            {{"CLK", logicBit(true)}},
            registerOutputs(fields, true, 0x100),
            CheckpointKind::AfterEdge),
        drive(registerInputs(
            fields, false, false, false, false, false, 0x200)),
        checkpoint(
            "hold-with-write-disabled",
            {{"CLK", logicBit(true)}},
            registerOutputs(fields, true, 0x100),
            CheckpointKind::AfterEdge),
        drive(registerInputs(
            fields, false, false, true, true, true, 0x300)),
        checkpoint(
            "flush-invalidates-entry",
            {{"CLK", logicBit(true)}},
            registerOutputs(fields, false, 0x300),
            CheckpointKind::AfterEdge),
        drive(registerInputs(
            fields, false, false, true, false, true, 0x400)),
        checkpoint(
            "capture-after-flush",
            {{"CLK", logicBit(true)}},
            registerOutputs(fields, true, 0x400),
            CheckpointKind::AfterEdge),
        checkpoint(
            "asynchronous-reset-recovery",
            {{"RST", logicBit(true)}},
            registerOutputs(fields, false, 0)),
    };

    return {
        std::move(test_id),
        std::string(family.id()),
        "PIPELINE_REGISTER_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues forwardingInputs(
    uint8_t rs1,
    uint8_t rs2,
    uint8_t ex_rd,
    bool ex_valid,
    bool ex_write,
    bool ex_ready,
    uint8_t mem_rd,
    bool mem_valid,
    bool mem_write) {
    return {
        {"ID_EX_RS1", logicBits(5, rs1)},
        {"ID_EX_RS2", logicBits(5, rs2)},
        {"EX_MEM_RD", logicBits(5, ex_rd)},
        {"EX_MEM_VALID", logicBit(ex_valid)},
        {"EX_MEM_REG_WRITE", logicBit(ex_write)},
        {"EX_MEM_RESULT_READY", logicBit(ex_ready)},
        {"MEM_WB_RD", logicBits(5, mem_rd)},
        {"MEM_WB_VALID", logicBit(mem_valid)},
        {"MEM_WB_REG_WRITE", logicBit(mem_write)},
    };
}

NamedValues forwardingOutputs(uint8_t a, uint8_t b) {
    return {
        {"FORWARD_A", logicBits(2, a)},
        {"FORWARD_B", logicBits(2, b)},
    };
}

ComponentTestSpec forwardingSpec() {
    ActionScenario scenario{
        "truth-table",
        {},
        {
            checkpoint(
                "no-producer",
                forwardingInputs(
                    1, 2, 0, false, false, false,
                    0, false, false),
                forwardingOutputs(0, 0)),
            checkpoint(
                "ex-forwards-both",
                forwardingInputs(
                    5, 5, 5, true, true, true,
                    0, false, false),
                forwardingOutputs(1, 1)),
            checkpoint(
                "mem-forwards-both",
                forwardingInputs(
                    7, 7, 0, false, false, false,
                    7, true, true),
                forwardingOutputs(2, 2)),
            checkpoint(
                "newest-producer-wins",
                forwardingInputs(
                    8, 8, 8, true, true, true,
                    8, true, true),
                forwardingOutputs(1, 1)),
            checkpoint(
                "unready-newest-blocks-stale-value",
                forwardingInputs(
                    9, 9, 9, true, true, false,
                    9, true, true),
                forwardingOutputs(0, 0)),
            checkpoint(
                "independent-operands",
                forwardingInputs(
                    10, 11, 10, true, true, true,
                    11, true, true),
                forwardingOutputs(1, 2)),
            checkpoint(
                "x0-never-forwards",
                forwardingInputs(
                    0, 0, 0, true, true, true,
                    0, true, true),
                forwardingOutputs(0, 0)),
        },
        {100'000, 2'000'000},
    };
    return {
        "RV32IForwardingUnitTest",
        std::string(circuit::families::RV32IForwardingUnit.id()),
        "RV32I_FORWARDING_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues hazardInputs(
    bool id_valid,
    uint8_t rs1,
    uint8_t rs2,
    bool uses_rs1,
    bool uses_rs2,
    bool ex_valid,
    bool ex_mem_read,
    uint8_t ex_rd) {
    return {
        {"ID_VALID", logicBit(id_valid)},
        {"ID_RS1", logicBits(5, rs1)},
        {"ID_RS2", logicBits(5, rs2)},
        {"ID_USES_RS1", logicBit(uses_rs1)},
        {"ID_USES_RS2", logicBit(uses_rs2)},
        {"EX_VALID", logicBit(ex_valid)},
        {"EX_MEM_READ", logicBit(ex_mem_read)},
        {"EX_RD", logicBits(5, ex_rd)},
    };
}

NamedValues hazardOutputs(bool hazard) {
    return {
        {"LOAD_USE_HAZARD", logicBit(hazard)},
        {"STALL_FETCH", logicBit(hazard)},
        {"STALL_IF_ID", logicBit(hazard)},
        {"BUBBLE_ID_EX", logicBit(hazard)},
    };
}

ComponentTestSpec hazardSpec() {
    ActionScenario scenario{
        "truth-table",
        {},
        {
            checkpoint(
                "no-valid-instructions",
                hazardInputs(
                    false, 1, 2, true, true,
                    false, true, 1),
                hazardOutputs(false)),
            checkpoint(
                "rs1-load-use",
                hazardInputs(
                    true, 5, 2, true, false,
                    true, true, 5),
                hazardOutputs(true)),
            checkpoint(
                "rs2-load-use",
                hazardInputs(
                    true, 1, 6, false, true,
                    true, true, 6),
                hazardOutputs(true)),
            checkpoint(
                "unused-encoded-field-does-not-stall",
                hazardInputs(
                    true, 7, 8, false, false,
                    true, true, 7),
                hazardOutputs(false)),
            checkpoint(
                "alu-producer-does-not-stall",
                hazardInputs(
                    true, 9, 0, true, false,
                    true, false, 9),
                hazardOutputs(false)),
            checkpoint(
                "different-register-does-not-stall",
                hazardInputs(
                    true, 10, 11, true, true,
                    true, true, 12),
                hazardOutputs(false)),
            checkpoint(
                "x0-never-stalls",
                hazardInputs(
                    true, 0, 0, true, true,
                    true, true, 0),
                hazardOutputs(false)),
        },
        {100'000, 2'000'000},
    };
    return {
        "RV32IHazardDetectionUnitTest",
        std::string(
            circuit::families::RV32IHazardDetectionUnit.id()),
        "RV32I_HAZARD_DETECTION_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues controlFlowInputs(
    uint32_t pc,
    uint32_t rs1,
    uint32_t immediate,
    uint8_t branch,
    uint8_t jump,
    bool eq,
    bool lt_signed,
    bool lt_unsigned) {
    return {
        {"PC", logicBits(32, pc)},
        {"RS1_VALUE", logicBits(32, rs1)},
        {"IMMEDIATE", logicBits(32, immediate)},
        {"BRANCH_TYPE", logicBits(3, branch)},
        {"JUMP_TYPE", logicBits(2, jump)},
        {"EQ", logicBit(eq)},
        {"LT_SIGNED", logicBit(lt_signed)},
        {"LT_UNSIGNED", logicBit(lt_unsigned)},
    };
}

NamedValues controlFlowOutputs(
    uint32_t plus4,
    uint32_t next,
    bool branch_taken,
    bool redirect,
    bool misaligned) {
    return {
        {"PC_PLUS_4", logicBits(32, plus4)},
        {"NEXT_PC", logicBits(32, next)},
        {"BRANCH_TAKEN", logicBit(branch_taken)},
        {"REDIRECT", logicBit(redirect)},
        {"TARGET_MISALIGNED", logicBit(misaligned)},
    };
}

ComponentTestSpec pipelineControlFlowSpec() {
    ActionScenario scenario{
        "truth-table",
        {},
        {
            checkpoint(
                "sequential",
                controlFlowInputs(
                    0x100, 0, 0, 0, 0,
                    false, false, false),
                controlFlowOutputs(
                    0x104, 0x104, false, false, false)),
            checkpoint(
                "taken-branch",
                controlFlowInputs(
                    0x100, 0, 0x20, 1, 0,
                    true, false, false),
                controlFlowOutputs(
                    0x104, 0x120, true, true, false)),
            checkpoint(
                "not-taken-branch",
                controlFlowInputs(
                    0x100, 0, 0x20, 1, 0,
                    false, false, false),
                controlFlowOutputs(
                    0x104, 0x104, false, false, false)),
            checkpoint(
                "jal",
                controlFlowInputs(
                    0x100, 0, 0x40, 0, 1,
                    false, false, false),
                controlFlowOutputs(
                    0x104, 0x140, false, true, false)),
            checkpoint(
                "jalr-masks-bit-zero",
                controlFlowInputs(
                    0x100, 0x201, 2, 0, 2,
                    false, false, false),
                controlFlowOutputs(
                    0x104, 0x202, false, true, true)),
            checkpoint(
                "misaligned-taken-branch",
                controlFlowInputs(
                    0x100, 0, 2, 1, 0,
                    true, false, false),
                controlFlowOutputs(
                    0x104, 0x102, true, true, true)),
        },
        {200'000, 4'000'000},
    };
    return {
        "RV32IPipelineControlFlowUnitTest",
        std::string(
            circuit::families::RV32IPipelineControlFlowUnit.id()),
        "RV32I_PIPELINE_CONTROL_FLOW_ROOT",
        {},
        {std::move(scenario)},
    };
}

ComponentTestSpec memoryAlignmentSpec() {
    ActionScenario scenario{
        "truth-table",
        {},
        {
            checkpoint(
                "byte-any-address",
                {{"ADDRESS", logicBits(32, 0x103)},
                 {"SIZE", logicBits(2, 0)}},
                {{"MISALIGNED", logicBit(false)}}),
            checkpoint(
                "aligned-halfword",
                {{"ADDRESS", logicBits(32, 0x102)},
                 {"SIZE", logicBits(2, 1)}},
                {{"MISALIGNED", logicBit(false)}}),
            checkpoint(
                "misaligned-halfword",
                {{"ADDRESS", logicBits(32, 0x103)},
                 {"SIZE", logicBits(2, 1)}},
                {{"MISALIGNED", logicBit(true)}}),
            checkpoint(
                "aligned-word",
                {{"ADDRESS", logicBits(32, 0x104)},
                 {"SIZE", logicBits(2, 2)}},
                {{"MISALIGNED", logicBit(false)}}),
            checkpoint(
                "word-offset-one",
                {{"ADDRESS", logicBits(32, 0x105)},
                 {"SIZE", logicBits(2, 2)}},
                {{"MISALIGNED", logicBit(true)}}),
            checkpoint(
                "word-offset-two",
                {{"ADDRESS", logicBits(32, 0x106)},
                 {"SIZE", logicBits(2, 2)}},
                {{"MISALIGNED", logicBit(true)}}),
        },
        {100'000, 2'000'000},
    };
    return {
        "RV32IMemoryAlignmentUnitTest",
        std::string(
            circuit::families::RV32IMemoryAlignmentUnit.id()),
        "RV32I_MEMORY_ALIGNMENT_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues retirementInputs(
    bool enable,
    bool valid,
    bool pc_misaligned = false,
    bool instruction_fault = false,
    bool legal = true,
    bool trap_request = false,
    uint8_t decode_cause = 0,
    bool mem_read = false,
    bool mem_write = false,
    bool data_misaligned = false,
    bool data_fault = false,
    bool target_misaligned = false,
    bool halt = false,
    bool reg_write = false) {
    return {
        {"ENABLE", logicBit(enable)},
        {"MEM_WB_VALID", logicBit(valid)},
        {"PC_MISALIGNED", logicBit(pc_misaligned)},
        {"INSTRUCTION_FAULT", logicBit(instruction_fault)},
        {"LEGAL", logicBit(legal)},
        {"TRAP_REQUEST", logicBit(trap_request)},
        {"DECODE_TRAP_CAUSE", logicBits(4, decode_cause)},
        {"MEM_READ", logicBit(mem_read)},
        {"MEM_WRITE", logicBit(mem_write)},
        {"DATA_MISALIGNED", logicBit(data_misaligned)},
        {"DATA_FAULT", logicBit(data_fault)},
        {"TARGET_MISALIGNED", logicBit(target_misaligned)},
        {"HALT_REQUEST", logicBit(halt)},
        {"REG_WRITE_REQUEST", logicBit(reg_write)},
    };
}

NamedValues retirementOutputs(
    bool commit,
    bool normal,
    bool reg_write,
    bool halt,
    bool trap,
    uint8_t cause) {
    return {
        {"COMMIT", logicBit(commit)},
        {"NORMAL_COMMIT", logicBit(normal)},
        {"REGISTER_WRITE", logicBit(reg_write)},
        {"HALT_EVENT", logicBit(halt)},
        {"TRAP_EVENT", logicBit(trap)},
        {"TRAP_CAUSE", logicBits(4, cause)},
    };
}

ComponentTestSpec pipelineRetirementSpec() {
    ActionScenario scenario{
        "priority-and-commit",
        {},
        {
            checkpoint(
                "invalid-stage",
                retirementInputs(true, false),
                retirementOutputs(
                    false, false, false, false, false, 0)),
            checkpoint(
                "normal-register-write",
                retirementInputs(
                    true, true, false, false, true, false,
                    0, false, false, false, false, false,
                    false, true),
                retirementOutputs(
                    true, true, true, false, false, 0)),
            checkpoint(
                "halt",
                retirementInputs(
                    true, true, false, false, true, false,
                    0, false, false, false, false, false,
                    true, false),
                retirementOutputs(
                    true, false, false, true, false, 0)),
            checkpoint(
                "environment-call",
                retirementInputs(
                    true, true, false, false, true, true, 2),
                retirementOutputs(
                    true, false, false, false, true, 2)),
            checkpoint(
                "illegal-instruction",
                retirementInputs(
                    true, true, false, false, false),
                retirementOutputs(
                    true, false, false, false, true, 1)),
            checkpoint(
                "load-address-misaligned",
                retirementInputs(
                    true, true, false, false, true, false,
                    0, true, false, true),
                retirementOutputs(
                    true, false, false, false, true, 5)),
            checkpoint(
                "store-access-fault",
                retirementInputs(
                    true, true, false, false, true, false,
                    0, false, true, false, true),
                retirementOutputs(
                    true, false, false, false, true, 8)),
            checkpoint(
                "pc-misalignment-has-highest-priority",
                retirementInputs(
                    true, true, true, true, false, true, 2,
                    false, true, true, true, true, true, true),
                retirementOutputs(
                    true, false, false, false, true, 3)),
        },
        {200'000, 4'000'000},
    };
    return {
        "RV32IPipelineRetirementUnitTest",
        std::string(
            circuit::families::RV32IPipelineRetirementUnit.id()),
        "RV32I_PIPELINE_RETIREMENT_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues coordinatorInputs(
    bool enable = true,
    bool halted = false,
    bool trapped = false,
    bool imem_ready = true,
    bool dmem_ready = true,
    bool hazard = false,
    bool terminal_pending = false,
    bool ex_preterminal = false,
    bool ex_redirect = false,
    bool memory_fault = false,
    bool mem_store = false,
    bool mem_load = false,
    bool retire_terminal = false) {
    return {
        {"ENABLE", logicBit(enable)},
        {"HALTED", logicBit(halted)},
        {"TRAPPED", logicBit(trapped)},
        {"IMEM_READY", logicBit(imem_ready)},
        {"DMEM_READY", logicBit(dmem_ready)},
        {"LOAD_USE_HAZARD", logicBit(hazard)},
        {"TERMINAL_PENDING", logicBit(terminal_pending)},
        {"EX_PRETERMINAL", logicBit(ex_preterminal)},
        {"MEMORY_TERMINAL", logicBit(false)},
        {"WB_TERMINAL", logicBit(false)},
        {"EX_REDIRECT", logicBit(ex_redirect)},
        {"MEMORY_FAULT", logicBit(memory_fault)},
        {"MEM_STORE", logicBit(mem_store)},
        {"MEM_LOAD", logicBit(mem_load)},
        {"RETIRE_TERMINAL", logicBit(retire_terminal)},
    };
}

NamedValues coordinatorOutputs(
    bool active,
    bool imem_read,
    bool dmem_read,
    bool dmem_write,
    bool fetch_valid,
    bool fetch_pc_write,
    bool fetch_redirect,
    bool if_write,
    bool if_flush,
    bool id_write,
    bool id_flush,
    bool ex_write,
    bool ex_flush,
    bool mem_write,
    bool mem_flush,
    bool commit_enable,
    bool stall,
    bool load_use_stall = false,
    bool memory_stall = false,
    bool data_port_stall = false,
    bool pipeline_flush = false) {
    return {
        {"ACTIVE", logicBit(active)},
        {"IMEM_READ_ENABLE", logicBit(imem_read)},
        {"DMEM_READ_ENABLE", logicBit(dmem_read)},
        {"DMEM_WRITE_ENABLE", logicBit(dmem_write)},
        {"FETCH_VALID", logicBit(fetch_valid)},
        {"FETCH_PC_WRITE", logicBit(fetch_pc_write)},
        {"FETCH_PC_REDIRECT", logicBit(fetch_redirect)},
        {"IF_ID_WRITE", logicBit(if_write)},
        {"IF_ID_FLUSH", logicBit(if_flush)},
        {"ID_EX_WRITE", logicBit(id_write)},
        {"ID_EX_FLUSH", logicBit(id_flush)},
        {"EX_MEM_WRITE", logicBit(ex_write)},
        {"EX_MEM_FLUSH", logicBit(ex_flush)},
        {"MEM_WB_WRITE", logicBit(mem_write)},
        {"MEM_WB_FLUSH", logicBit(mem_flush)},
        {"COMMIT_ENABLE", logicBit(commit_enable)},
        {"PIPELINE_STALL", logicBit(stall)},
        {"LOAD_USE_STALL", logicBit(load_use_stall)},
        {"MEMORY_STALL", logicBit(memory_stall)},
        {"DATA_PORT_STALL", logicBit(data_port_stall)},
        {"PIPELINE_FLUSH", logicBit(pipeline_flush)},
    };
}

ComponentTestSpec pipelineCoordinatorSpec() {
    ActionScenario scenario{
        "control-cases",
        {},
        {
            checkpoint(
                "disabled",
                coordinatorInputs(false),
                coordinatorOutputs(
                    false, false, false, false, false, false,
                    false, false, false, false, false, false,
                    false, false, false, false, false)),
            checkpoint(
                "normal-fetch-and-advance",
                coordinatorInputs(),
                coordinatorOutputs(
                    true, true, false, false, true, true,
                    false, true, false, true, false, true,
                    false, true, false, true, false)),
            checkpoint(
                "load-use-bubble",
                coordinatorInputs(
                    true, false, false, true, true, true),
                coordinatorOutputs(
                    true, true, false, false, false, false,
                    false, false, false, true, true, true,
                    false, true, false, true, true,
                    true, false, false, false)),
            checkpoint(
                "redirect-flushes-younger-stages",
                coordinatorInputs(
                    true, false, false, true, true, false,
                    false, false, true),
                coordinatorOutputs(
                    true, true, false, false, false, true,
                    true, true, true, true, true, true,
                    false, true, false, true, false,
                    false, false, false, true)),
            checkpoint(
                "older-memory-fault-kills-ex-stage",
                coordinatorInputs(
                    true, false, false, true, true, false,
                    false, false, false, true),
                coordinatorOutputs(
                    true, true, false, false, false, false,
                    false, true, true, true, true, true,
                    true, true, false, true, false,
                    false, false, false, true)),
            checkpoint(
                "memory-wait-holds-pipeline",
                coordinatorInputs(
                    true, false, false, true, false, false,
                    false, false, false, false, false, true),
                coordinatorOutputs(
                    true, true, true, false, false, false,
                    false, false, false, false, false, false,
                    false, false, false, false, true,
                    false, true, false, false)),
            checkpoint(
                "invalid-dual-memory-request-stalls",
                coordinatorInputs(
                    true, false, false, true, true, false,
                    false, false, false, false, true, true),
                coordinatorOutputs(
                    true, true, false, true, false, false,
                    false, false, false, false, false, false,
                    false, true, true, true, true,
                    false, false, true, false)),
            checkpoint(
                "terminal-retirement-clears-pipeline",
                coordinatorInputs(
                    true, false, false, true, true, false,
                    true, false, false, false, false, false,
                    true),
                coordinatorOutputs(
                    true, false, false, false, false, false,
                    false, true, true, true, true, true,
                    true, true, true, true, false)),
        },
        {200'000, 4'000'000},
    };
    return {
        "RV32IPipelineCoordinatorTest",
        std::string(
            circuit::families::RV32IPipelineCoordinator.id()),
        "RV32I_PIPELINE_COORDINATOR_ROOT",
        {},
        {std::move(scenario)},
    };
}

uint32_t controlWord(uint32_t instruction) {
    return rv32i::pipeline_encoding::packControl(
        rv32i::RV32IControl::fromRaw(instruction));
}

uint32_t addressWord(uint32_t instruction) {
    const auto decoded = rv32i::RV32IDecoder::decode(instruction);
    return rv32i::pipeline_encoding::packAddresses(
        decoded.rs1, decoded.rs2, decoded.rd);
}

ComponentTestSpec fetchStageSpec() {
    ActionScenario scenario{
        "pc-and-fetch-payload",
        {},
        {
            checkpoint(
                "reset",
                {{"CLK", logicBit(false)},
                 {"RST", logicBit(true)},
                 {"PC_WRITE", logicBit(false)},
                 {"PC_REDIRECT", logicBit(false)},
                 {"FETCH_VALID", logicBit(false)},
                 {"REDIRECT_PC", logicBits(32, 0)},
                 {"IMEM_READ_DATA", logicBits(32, 0x00500093U)},
                 {"IMEM_FAULT", logicBit(false)}},
                {{"FETCH_PC", logicBits(32, 0)},
                 {"IMEM_ADDR", logicBits(32, 0)},
                 {"OUT_VALID", logicBit(false)},
                 {"OUT_PC", logicBits(32, 0)},
                 {"OUT_INSTRUCTION", logicBits(32, 0)},
                 {"OUT_FETCH_STATUS", logicBits(32, 0)}}),
            checkpoint(
                "fetch-at-zero",
                {{"RST", logicBit(false)},
                 {"FETCH_VALID", logicBit(true)}},
                {{"FETCH_PC", logicBits(32, 0)},
                 {"OUT_VALID", logicBit(true)},
                 {"OUT_PC", logicBits(32, 0)},
                 {"OUT_INSTRUCTION", logicBits(32, 0x00500093U)},
                 {"OUT_FETCH_STATUS", logicBits(32, 0)}}),
            drive(
                {{"CLK", logicBit(false)},
                 {"PC_WRITE", logicBit(true)},
                 {"PC_REDIRECT", logicBit(true)},
                 {"REDIRECT_PC", logicBits(32, 2)}}),
            checkpoint(
                "misaligned-redirect",
                {{"CLK", logicBit(true)}},
                {{"FETCH_PC", logicBits(32, 2)},
                 {"IMEM_ADDR", logicBits(32, 2)},
                 {"OUT_PC", logicBits(32, 2)},
                 {"OUT_FETCH_STATUS", logicBits(32, 1)}},
                CheckpointKind::AfterEdge),
        },
        {200'000, 4'000'000},
    };
    return {
        "RV32IFetchStageTest",
        std::string(circuit::families::RV32IFetchStage.id()),
        "RV32I_FETCH_STAGE_ROOT",
        {},
        {std::move(scenario)},
    };
}

ComponentTestSpec decodeStageSpec() {
    constexpr uint32_t addi_x3_x1_5 = 0x00508193U;
    const auto control = controlWord(addi_x3_x1_5);
    const auto addresses = addressWord(addi_x3_x1_5);
    ActionScenario scenario{
        "decode-bypass-and-hazard",
        {},
        {
            checkpoint(
                "reset-and-decode",
                {{"CLK", logicBit(false)},
                 {"RST", logicBit(true)},
                 {"IN_VALID", logicBit(true)},
                 {"IN_PC", logicBits(32, 0x100)},
                 {"IN_INSTRUCTION", logicBits(32, addi_x3_x1_5)},
                 {"IN_FETCH_STATUS", logicBits(32, 0)},
                 {"EX_CONTROL", logicBits(32, 0)},
                 {"EX_REGISTER_ADDRESSES", logicBits(32, 0)},
                 {"EX_VALID", logicBit(false)},
                 {"WB_VALID", logicBit(true)},
                 {"WB_REG_WRITE", logicBit(true)},
                 {"WB_REGISTER_WRITE", logicBit(false)},
                 {"WB_RD", logicBits(5, 1)},
                 {"WB_VALUE", logicBits(32, 9)}},
                {{"OUT_VALID", logicBit(true)},
                 {"OUT_PC", logicBits(32, 0x100)},
                 {"OUT_INSTRUCTION", logicBits(32, addi_x3_x1_5)},
                 {"OUT_RS1_VALUE", logicBits(32, 9)},
                 {"OUT_RS2_VALUE", logicBits(32, 0)},
                 {"OUT_IMMEDIATE", logicBits(32, 5)},
                 {"OUT_CONTROL", logicBits(32, control)},
                 {"OUT_REGISTER_ADDRESSES", logicBits(32, addresses)},
                 {"OUT_FETCH_STATUS", logicBits(32, 0)},
                 {"LOAD_USE_HAZARD", logicBit(false)},
                 {"TERMINAL", logicBit(false)}}),
            checkpoint(
                "load-use-hazard",
                {{"RST", logicBit(false)},
                 {"EX_VALID", logicBit(true)},
                 {"EX_CONTROL", logicBits(
                     32,
                     control
                         | (1U << 2))},
                 {"EX_REGISTER_ADDRESSES", logicBits(
                     32,
                     rv32i::pipeline_encoding::packAddresses(
                         0, 0, 1))}},
                {{"LOAD_USE_HAZARD", logicBit(true)}}),
            checkpoint(
                "instruction-fault-terminal",
                {{"IN_FETCH_STATUS", logicBits(32, 2)}},
                {{"TERMINAL", logicBit(true)}}),
        },
        {300'000, 5'000'000},
    };
    return {
        "RV32IDecodeStageTest",
        std::string(circuit::families::RV32IDecodeStage.id()),
        "RV32I_DECODE_STAGE_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues executeInputs(
    uint32_t instruction,
    uint32_t pc,
    uint32_t rs1_value,
    uint32_t rs2_value) {
    const auto decoded = rv32i::RV32IDecoder::decode(instruction);
    return {
        {"IN_VALID", logicBit(true)},
        {"IN_PC", logicBits(32, pc)},
        {"IN_INSTRUCTION", logicBits(32, instruction)},
        {"IN_RS1_VALUE", logicBits(32, rs1_value)},
        {"IN_RS2_VALUE", logicBits(32, rs2_value)},
        {"IN_IMMEDIATE", logicBits(
            32, static_cast<uint32_t>(decoded.immediate))},
        {"IN_CONTROL", logicBits(32, controlWord(instruction))},
        {"IN_REGISTER_ADDRESSES", logicBits(
            32, addressWord(instruction))},
        {"IN_FETCH_STATUS", logicBits(32, 0)},
        {"EX_MEM_VALID", logicBit(false)},
        {"EX_MEM_RD", logicBits(5, 0)},
        {"EX_MEM_REG_WRITE", logicBit(false)},
        {"EX_MEM_RESULT_READY", logicBit(false)},
        {"EX_MEM_VALUE", logicBits(32, 0)},
        {"MEM_WB_VALID", logicBit(false)},
        {"MEM_WB_RD", logicBits(5, 0)},
        {"MEM_WB_REG_WRITE", logicBit(false)},
        {"MEM_WB_VALUE", logicBits(32, 0)},
    };
}

ComponentTestSpec executeStageSpec() {
    constexpr uint32_t addi_x3_x1_5 = 0x00508193U;
    constexpr uint32_t beq_x1_x2_plus8 = 0x00208463U;
    ActionScenario scenario{
        "alu-forward-and-redirect",
        {},
        {
            checkpoint(
                "addi",
                executeInputs(addi_x3_x1_5, 0x100, 7, 0),
                {{"OUT_VALID", logicBit(true)},
                 {"OUT_PC", logicBits(32, 0x100)},
                 {"OUT_ALU_RESULT", logicBits(32, 12)},
                 {"OUT_STORE_DATA", logicBits(32, 0)},
                 {"OUT_PC_PLUS_4", logicBits(32, 0x104)},
                 {"OUT_NEXT_PC", logicBits(32, 0x104)},
                 {"OUT_EXECUTION_STATUS", logicBits(32, 0)},
                 {"REDIRECT", logicBit(false)},
                 {"PRETERMINAL", logicBit(false)}}),
            checkpoint(
                "ex-forwarding",
                {{"EX_MEM_VALID", logicBit(true)},
                 {"EX_MEM_RD", logicBits(5, 1)},
                 {"EX_MEM_REG_WRITE", logicBit(true)},
                 {"EX_MEM_RESULT_READY", logicBit(true)},
                 {"EX_MEM_VALUE", logicBits(32, 20)}},
                {{"OUT_ALU_RESULT", logicBits(32, 25)}}),
            checkpoint(
                "taken-branch",
                executeInputs(beq_x1_x2_plus8, 0x200, 4, 4),
                {{"OUT_NEXT_PC", logicBits(32, 0x208)},
                 {"REDIRECT", logicBit(true)},
                 {"REDIRECT_PC", logicBits(32, 0x208)},
                 {"PRETERMINAL", logicBit(false)}}),
        },
        {300'000, 5'000'000},
    };
    return {
        "RV32IExecuteStageTest",
        std::string(circuit::families::RV32IExecuteStage.id()),
        "RV32I_EXECUTE_STAGE_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues memoryInputs(uint32_t address) {
    constexpr uint32_t lw_x1_0_x2 = 0x00012083U;
    return {
        {"IN_VALID", logicBit(true)},
        {"IN_PC", logicBits(32, 0x100)},
        {"IN_INSTRUCTION", logicBits(32, lw_x1_0_x2)},
        {"IN_ALU_RESULT", logicBits(32, address)},
        {"IN_STORE_DATA", logicBits(32, 0)},
        {"IN_PC_PLUS_4", logicBits(32, 0x104)},
        {"IN_NEXT_PC", logicBits(32, 0x104)},
        {"IN_CONTROL", logicBits(32, controlWord(lw_x1_0_x2))},
        {"IN_REGISTER_ADDRESSES", logicBits(
            32, addressWord(lw_x1_0_x2))},
        {"IN_FETCH_STATUS", logicBits(32, 0)},
        {"IN_EXECUTION_STATUS", logicBits(32, 0)},
        {"DMEM_READ_DATA", logicBits(32, 0x12345678)},
        {"DMEM_FAULT", logicBit(false)},
        {"DMEM_READ_ENABLE", logicBit(true)},
        {"DMEM_WRITE_ENABLE", logicBit(false)},
    };
}

ComponentTestSpec memoryStageSpec() {
    ActionScenario scenario{
        "load-alignment-and-port",
        {},
        {
            checkpoint(
                "aligned-load",
                memoryInputs(0x100),
                {{"OUT_VALID", logicBit(true)},
                 {"OUT_MEMORY_DATA", logicBits(32, 0x12345678)},
                 {"OUT_MEMORY_STATUS", logicBits(32, 0)},
                 {"LOAD_REQUEST", logicBit(true)},
                 {"STORE_REQUEST", logicBit(false)},
                 {"LOAD_FAULT", logicBit(false)},
                 {"STORE_FAULT", logicBit(false)},
                 {"MEMORY_FAULT", logicBit(false)},
                 {"PRETERMINAL", logicBit(false)},
                 {"FORWARD_RESULT_READY", logicBit(false)},
                 {"DMEM_ADDR", logicBits(32, 0x100)},
                 {"DMEM_SIZE", logicBits(2, 2)},
                 {"DMEM_SIGN_EXTEND", logicBit(false)}}),
            checkpoint(
                "misaligned-load",
                {{"IN_ALU_RESULT", logicBits(32, 0x102)}},
                {{"OUT_MEMORY_STATUS", logicBits(32, 1)},
                 {"LOAD_REQUEST", logicBit(false)},
                 {"LOAD_FAULT", logicBit(true)},
                 {"MEMORY_FAULT", logicBit(true)}}),
            checkpoint(
                "aligned-store-issued-in-memory-stage",
                {{"IN_INSTRUCTION", logicBits(32, 0x00312023U)},
                 {"IN_ALU_RESULT", logicBits(32, 0x100)},
                 {"IN_STORE_DATA", logicBits(32, 0x89abcdefU)},
                 {"IN_CONTROL", logicBits(
                     32, controlWord(0x00312023U))},
                 {"DMEM_READ_ENABLE", logicBit(false)},
                 {"DMEM_WRITE_ENABLE", logicBit(true)}},
                {{"OUT_MEMORY_STATUS", logicBits(32, 0)},
                 {"LOAD_REQUEST", logicBit(false)},
                 {"STORE_REQUEST", logicBit(true)},
                 {"STORE_FAULT", logicBit(false)},
                 {"MEMORY_FAULT", logicBit(false)},
                 {"DMEM_ADDR", logicBits(32, 0x100)},
                 {"DMEM_WRITE_DATA", logicBits(32, 0x89abcdefU)},
                 {"DMEM_SIZE", logicBits(2, 2)}}),
            checkpoint(
                "misaligned-store-is-not-issued",
                {{"IN_ALU_RESULT", logicBits(32, 0x102)}},
                {{"OUT_MEMORY_STATUS", logicBits(32, 1)},
                 {"STORE_REQUEST", logicBit(false)},
                 {"STORE_FAULT", logicBit(true)},
                 {"MEMORY_FAULT", logicBit(true)}}),
        },
        {300'000, 5'000'000},
    };
    return {
        "RV32IMemoryStageTest",
        std::string(circuit::families::RV32IMemoryStage.id()),
        "RV32I_MEMORY_STAGE_ROOT",
        {},
        {std::move(scenario)},
    };
}

ComponentTestSpec writebackStageSpec() {
    constexpr uint32_t addi_x1_x0_5 = 0x00500093U;
    ActionScenario scenario{
        "commit-and-state",
        {},
        {
            checkpoint(
                "reset",
                {{"CLK", logicBit(false)},
                 {"RST", logicBit(true)},
                 {"COMMIT_ENABLE", logicBit(true)},
                 {"IN_VALID", logicBit(false)},
                 {"IN_PC", logicBits(32, 0)},
                 {"IN_INSTRUCTION", logicBits(32, addi_x1_x0_5)},
                 {"IN_ALU_RESULT", logicBits(32, 5)},
                 {"IN_MEMORY_DATA", logicBits(32, 0)},
                 {"IN_STORE_DATA", logicBits(32, 0)},
                 {"IN_PC_PLUS_4", logicBits(32, 4)},
                 {"IN_NEXT_PC", logicBits(32, 4)},
                 {"IN_CONTROL", logicBits(
                     32, controlWord(addi_x1_x0_5))},
                 {"IN_REGISTER_ADDRESSES", logicBits(
                     32, addressWord(addi_x1_x0_5))},
                 {"IN_FETCH_STATUS", logicBits(32, 0)},
                 {"IN_EXECUTION_STATUS", logicBits(32, 0)},
                 {"IN_MEMORY_STATUS", logicBits(32, 0)}},
                {{"PC", logicBits(32, 0)},
                 {"HALTED", logicBit(false)},
                 {"TRAPPED", logicBit(false)},
                 {"RETIRED_COUNT", logicBits(32, 0)},
                 {"COMMIT_VALID", logicBit(false)}}),
            drive(
                {{"RST", logicBit(false)},
                 {"IN_VALID", logicBit(true)}}),
            checkpoint(
                "commit-addi",
                {{"CLK", logicBit(true)}},
                {{"PC", logicBits(32, 4)},
                 {"RETIRED_COUNT", logicBits(32, 1)},
                 {"RETIRED_PC", logicBits(32, 0)},
                 {"RETIRED_INSTRUCTION", logicBits(
                     32, addi_x1_x0_5)},
                 {"WB_VALUE", logicBits(32, 5)},
                 {"WB_RD", logicBits(5, 1)},
                 {"REGISTER_WRITE", logicBit(true)},
                 {"COMMIT_VALID", logicBit(true)},
                 {"TERMINAL", logicBit(false)}},
                CheckpointKind::InstructionCommit),
        },
        {300'000, 5'000'000},
    };
    return {
        "RV32IWritebackStageTest",
        std::string(circuit::families::RV32IWritebackStage.id()),
        "RV32I_WRITEBACK_STAGE_ROOT",
        {},
        {std::move(scenario)},
    };
}

NamedValues coreInputs(
    bool clk,
    bool rst,
    uint32_t instruction) {
    return {
        {"CLK", logicBit(clk)},
        {"RST", logicBit(rst)},
        {"ENABLE", logicBit(true)},
        {"IMEM_READ_DATA", logicBits(32, instruction)},
        {"IMEM_READY", logicBit(true)},
        {"IMEM_FAULT", logicBit(false)},
        {"DMEM_READ_DATA", logicBits(32, 0)},
        {"DMEM_READY", logicBit(true)},
        {"DMEM_FAULT", logicBit(false)},
    };
}

NamedValues coreStageOutputs(
    uint32_t retired,
    bool halted,
    bool if_valid,
    uint32_t if_pc,
    bool id_valid,
    uint32_t id_pc,
    bool ex_valid,
    uint32_t ex_pc,
    bool mem_valid,
    uint32_t mem_pc) {
    return {
        {"RETIRED_COUNT", logicBits(32, retired)},
        {"HALTED", logicBit(halted)},
        {"TRAPPED", logicBit(false)},
        {"TRAP_CAUSE", logicBits(4, 0)},
        {"IF_ID_VALID", logicBit(if_valid)},
        {"IF_ID_PC", logicBits(32, if_pc)},
        {"ID_EX_VALID", logicBit(id_valid)},
        {"ID_EX_PC", logicBits(32, id_pc)},
        {"EX_MEM_VALID", logicBit(ex_valid)},
        {"EX_MEM_PC", logicBits(32, ex_pc)},
        {"MEM_WB_VALID", logicBit(mem_valid)},
        {"MEM_WB_PC", logicBits(32, mem_pc)},
    };
}

ComponentTestSpec fiveStageCoreSpec() {
    constexpr uint32_t addi_x1 = 0x00500093U;
    constexpr uint32_t addi_x2 = 0x00708113U;
    constexpr uint32_t add_x3 = 0x002081B3U;
    constexpr uint32_t ebreak = 0x00100073U;

    ActionScenario scenario{
        "forwarding-and-retirement",
        {},
        {
            checkpoint(
                "reset",
                coreInputs(false, true, addi_x1),
                coreStageOutputs(
                    0, false,
                    false, 0,
                    false, 0,
                    false, 0,
                    false, 0)),
            checkpoint(
                "release-reset",
                {{"RST", logicBit(false)}},
                coreStageOutputs(
                    0, false,
                    false, 0,
                    false, 0,
                    false, 0,
                    false, 0)),
            checkpoint(
                "fetch-first",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    0, false,
                    true, 0,
                    false, 0,
                    false, 0,
                    false, 0),
                CheckpointKind::AfterEdge),
            drive(coreInputs(false, false, addi_x2)),
            checkpoint(
                "decode-first",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    0, false,
                    true, 4,
                    true, 0,
                    false, 0,
                    false, 0),
                CheckpointKind::AfterEdge),
            drive(coreInputs(false, false, add_x3)),
            checkpoint(
                "execute-first",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    0, false,
                    true, 8,
                    true, 4,
                    true, 0,
                    false, 0),
                CheckpointKind::AfterEdge),
            drive(coreInputs(false, false, ebreak)),
            checkpoint(
                "fill-pipeline",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    0, false,
                    true, 12,
                    true, 8,
                    true, 4,
                    true, 0),
                CheckpointKind::AfterEdge),
            drive(coreInputs(false, false, 0)),
            checkpoint(
                "retire-first",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    1, false,
                    false, 0,
                    true, 12,
                    true, 8,
                    true, 4),
                CheckpointKind::InstructionCommit),
            drive({{"CLK", logicBit(false)}}),
            checkpoint(
                "retire-second",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    2, false,
                    false, 0,
                    false, 0,
                    true, 12,
                    true, 8),
                CheckpointKind::InstructionCommit),
            drive({{"CLK", logicBit(false)}}),
            checkpoint(
                "retire-third",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    3, false,
                    false, 0,
                    false, 0,
                    false, 0,
                    true, 12),
                CheckpointKind::InstructionCommit),
            drive({{"CLK", logicBit(false)}}),
            checkpoint(
                "halt-at-writeback",
                {{"CLK", logicBit(true)}},
                coreStageOutputs(
                    4, true,
                    false, 0,
                    false, 0,
                    false, 0,
                    false, 0),
                CheckpointKind::InstructionCommit),
        },
        {200'000, 4'000'000},
    };

    return {
        "RV32IFiveStageCoreTest",
        std::string(circuit::families::RV32IFiveStageCore.id()),
        "RV32I_FIVE_STAGE_CORE_ROOT",
        {},
        {std::move(scenario)},
    };
}
} // namespace

RV32IIFIDPipelineRegisterTest::RV32IIFIDPipelineRegisterTest()
    : ComponentScenarioTest(
          pipelineRegisterSpec(
              "RV32IIFIDPipelineRegisterTest",
              circuit::families::RV32IIFIDPipelineRegister,
              {"PC", "INSTRUCTION", "FETCH_STATUS"}),
          {},
          rv32iPreviewProfile()) {}

RV32IIDEXPipelineRegisterTest::RV32IIDEXPipelineRegisterTest()
    : ComponentScenarioTest(
          pipelineRegisterSpec(
              "RV32IIDEXPipelineRegisterTest",
              circuit::families::RV32IIDEXPipelineRegister,
              {"PC", "INSTRUCTION", "RS1_VALUE", "RS2_VALUE",
               "IMMEDIATE", "CONTROL", "REGISTER_ADDRESSES",
               "FETCH_STATUS"}),
          {},
          rv32iPreviewProfile()) {}

RV32IEXMEMPipelineRegisterTest::RV32IEXMEMPipelineRegisterTest()
    : ComponentScenarioTest(
          pipelineRegisterSpec(
              "RV32IEXMEMPipelineRegisterTest",
              circuit::families::RV32IEXMEMPipelineRegister,
              {"PC", "INSTRUCTION", "ALU_RESULT", "STORE_DATA",
               "PC_PLUS_4", "NEXT_PC", "CONTROL",
               "REGISTER_ADDRESSES", "FETCH_STATUS",
               "EXECUTION_STATUS"}),
          {},
          rv32iPreviewProfile()) {}

RV32IMEMWBPipelineRegisterTest::RV32IMEMWBPipelineRegisterTest()
    : ComponentScenarioTest(
          pipelineRegisterSpec(
              "RV32IMEMWBPipelineRegisterTest",
              circuit::families::RV32IMEMWBPipelineRegister,
              {"PC", "INSTRUCTION", "ALU_RESULT", "MEMORY_DATA",
               "STORE_DATA", "PC_PLUS_4", "NEXT_PC", "CONTROL",
               "REGISTER_ADDRESSES", "FETCH_STATUS",
               "EXECUTION_STATUS", "MEMORY_STATUS"}),
          {},
          rv32iPreviewProfile()) {}

RV32IForwardingUnitTest::RV32IForwardingUnitTest()
    : ComponentScenarioTest(
          forwardingSpec(), {}, rv32iPreviewProfile()) {}

RV32IHazardDetectionUnitTest::RV32IHazardDetectionUnitTest()
    : ComponentScenarioTest(
          hazardSpec(), {}, rv32iPreviewProfile()) {}

RV32IPipelineControlFlowUnitTest::
RV32IPipelineControlFlowUnitTest()
    : ComponentScenarioTest(
          pipelineControlFlowSpec(), {}, rv32iPreviewProfile()) {}

RV32IMemoryAlignmentUnitTest::
RV32IMemoryAlignmentUnitTest()
    : ComponentScenarioTest(
          memoryAlignmentSpec(), {}, rv32iPreviewProfile()) {}

RV32IPipelineRetirementUnitTest::
RV32IPipelineRetirementUnitTest()
    : ComponentScenarioTest(
          pipelineRetirementSpec(), {}, rv32iPreviewProfile()) {}

RV32IPipelineCoordinatorTest::
RV32IPipelineCoordinatorTest()
    : ComponentScenarioTest(
          pipelineCoordinatorSpec(), {}, rv32iPreviewProfile()) {}

RV32IFetchStageTest::RV32IFetchStageTest()
    : ComponentScenarioTest(
          fetchStageSpec(), {}, rv32iPreviewProfile()) {}

RV32IDecodeStageTest::RV32IDecodeStageTest()
    : ComponentScenarioTest(
          decodeStageSpec(), {}, rv32iPreviewProfile()) {}

RV32IExecuteStageTest::RV32IExecuteStageTest()
    : ComponentScenarioTest(
          executeStageSpec(), {}, rv32iPreviewProfile()) {}

RV32IMemoryStageTest::RV32IMemoryStageTest()
    : ComponentScenarioTest(
          memoryStageSpec(), {}, rv32iPreviewProfile()) {}

RV32IWritebackStageTest::RV32IWritebackStageTest()
    : ComponentScenarioTest(
          writebackStageSpec(), {}, rv32iPreviewProfile()) {}

RV32IFiveStageCoreTest::RV32IFiveStageCoreTest()
    : ComponentScenarioTest(
          fiveStageCoreSpec(),
          "forwarding-and-retirement",
          rv32i::architectureStructuralProfile(
              circuit::builtinComponentCatalog(),
              "rv32i-five-stage-core-preview")) {}

bool RV32IFiveStageCoreTest::run() {
    if (!ComponentScenarioTest::run()) {
        return false;
    }
    try {
        const auto& artifacts = getRunArtifacts();
        require(
            !artifacts.empty(),
            "five-stage core test produced no fidelity artifacts");
        for (const auto& artifact : artifacts) {
            const auto state_view =
                std::dynamic_pointer_cast<RV32IStateView>(
                    artifact.root);
            require(
                state_view != nullptr,
                "five-stage core lacks architectural-state view");
            const auto state =
                state_view->snapshotArchitecturalState().toKnownState();
            require(
                state.pc == 12
                    && state.x[1] == 5
                    && state.x[2] == 12
                    && state.x[3] == 17
                    && state.halted
                    && !state.trapped,
                "five-stage forwarding/retirement state is incorrect");
        }
        return true;
    } catch (const std::exception& error) {
        std::cerr
            << "[FAIL] Test 'RV32IFiveStageCoreTest' "
            << "architectural observation: "
            << error.what() << std::endl;
        return false;
    }
}
