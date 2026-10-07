#include "tests/MemoryComponentTests.hpp"

#include "components/Component.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/Register.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
uint32_t registerFilePattern(uint32_t reg_index) {
    return 0x10000000U | (reg_index * 0x01010101U);
}

circuit::test::ScenarioAction checkpointAction(
    std::string id,
    circuit::test::NamedValues inputs,
    circuit::test::NamedValues outputs,
    circuit::test::CheckpointKind kind =
        circuit::test::CheckpointKind::Settled,
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

circuit::test::ScenarioAction driveAction(
    circuit::test::NamedValues inputs) {
    circuit::test::ScenarioAction action;
    action.inputs = std::move(inputs);
    action.emit_checkpoint = false;
    return action;
}

circuit::test::LogicVector gateUnknownSelect(
    const circuit::test::LogicVector& values) {
    auto result = values;
    for (auto& value : result) {
        value = value == LogicValue::LOW
            ? LogicValue::LOW
            : LogicValue::UNKNOWN;
    }
    return result;
}

circuit::test::LogicVector gateUnknownWrite(
    const circuit::test::LogicVector& current,
    const circuit::test::LogicVector& data) {
    assert(current.size() == data.size());
    circuit::test::LogicVector result(
        current.size(), LogicValue::UNKNOWN);
    for (size_t bit_index = 0; bit_index < current.size(); ++bit_index) {
        if (current[bit_index] == LogicValue::LOW
            && data[bit_index] == LogicValue::LOW) {
            result[bit_index] = LogicValue::LOW;
        }
    }
    return result;
}

circuit::test::ComponentTestSpec flipFlopCellSpec() {
    using namespace circuit::test;

    ActionScenario scenario{
        "contract",
        {},
        {},
        {10'000, 200'000},
    };
    scenario.actions = {
        checkpointAction(
            "reset",
            {
                {"D", logicBit(false)},
                {"WE", logicBit(false)},
                {"CLK", logicBit(false)},
                {"RST", logicBit(true)},
            },
            {{"Q", logicBit(false)}},
            CheckpointKind::Settled,
            "Reset clears the stored bit."),
        driveAction({{"RST", logicBit(false)}}),
        driveAction({{"D", logicBit(true)}}),
        checkpointAction(
            "hold-disabled-edge",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(false)}},
            CheckpointKind::AfterEdge,
            "A rising edge does not write while WE is low."),
        driveAction({{"CLK", logicBit(false)}}),
        driveAction({{"WE", logicBit(true)}}),
        checkpointAction(
            "write-one",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(true)}},
            CheckpointKind::AfterEdge,
            "A rising edge captures D=1 while WE is high."),
        checkpointAction(
            "no-level-capture",
            {{"D", logicBit(false)}},
            {{"Q", logicBit(true)}},
            CheckpointKind::Settled,
            "Changing D while CLK stays high is not another edge."),
        driveAction({{"CLK", logicBit(false)}}),
        checkpointAction(
            "write-zero",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(false)}},
            CheckpointKind::AfterEdge,
            "The next rising edge captures D=0."),
        driveAction({{"CLK", logicBit(false)}}),
        driveAction({
            {"WE", logicBit(false)},
            {"D", logicBit(true)},
        }),
        checkpointAction(
            "hold-zero",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(false)}},
            CheckpointKind::AfterEdge,
            "WE=0 preserves zero."),
        driveAction({{"CLK", logicBit(false)}}),
        driveAction({
            {"WE", logicBit(true)},
            {"D", logicBit(LogicValue::UNKNOWN)},
        }),
        checkpointAction(
            "capture-unknown-data",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(LogicValue::UNKNOWN)}},
            CheckpointKind::AfterEdge,
            "Unknown data is stored as unknown."),
        driveAction({{"CLK", logicBit(false)}}),
        checkpointAction(
            "reset-clears-unknown",
            {{"RST", logicBit(true)}},
            {{"Q", logicBit(false)}},
            CheckpointKind::Settled,
            "Reset recovers from unknown state."),
        checkpointAction(
            "unknown-reset-preserves-zero",
            {{"RST", logicBit(LogicValue::UNKNOWN)}},
            {{"Q", logicBit(false)}},
            CheckpointKind::Settled,
            "An uncertain reset preserves zero because reset and hold agree."),
        driveAction({{"RST", logicBit(false)}}),
        driveAction({
            {"D", logicBit(false)},
            {"WE", logicBit(LogicValue::UNKNOWN)},
        }),
        checkpointAction(
            "unknown-write-enable-agrees",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(false)}},
            CheckpointKind::AfterEdge,
            "An uncertain write preserves zero when hold and write agree."),
        driveAction({{"CLK", logicBit(false)}}),
        driveAction({
            {"D", logicBit(true)},
            {"WE", logicBit(LogicValue::UNKNOWN)},
        }),
        checkpointAction(
            "unknown-write-enable",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(LogicValue::UNKNOWN)}},
            CheckpointKind::AfterEdge,
            "An ambiguous write attempt produces unknown state."),
        driveAction({{"CLK", logicBit(false)}}),
        checkpointAction(
            "reset-before-high-z",
            {{"RST", logicBit(true)}},
            {{"Q", logicBit(false)}}),
        driveAction({
            {"RST", logicBit(false)},
            {"WE", logicBit(true)},
            {"D", logicBit(LogicValue::HIGH_Z)},
        }),
        checkpointAction(
            "capture-high-z-as-unknown",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBit(LogicValue::UNKNOWN)}},
            CheckpointKind::AfterEdge,
            "A stored high-impedance input becomes unknown state."),
    };
    return {
        "FlipFlopCellTest",
        std::string(circuit::families::FlipFlopCell.id()),
        "FLIP_FLOP_CELL_ROOT",
        {},
        {std::move(scenario)},
    };
}

circuit::test::ComponentTestSpec register32Spec() {
    using namespace circuit::test;

    ActionScenario scenario{
        "contract",
        {},
        {},
        {50'000, 2'000'000},
    };
    scenario.actions = {
        checkpointAction(
            "reset",
            {
                {"D", logicBits(32, 0)},
                {"WE", logicBit(false)},
                {"CLK", logicBit(false)},
                {"RST", logicBit(true)},
            },
            {{"Q", logicBits(32, 0)}}),
        driveAction({{"RST", logicBit(false)}}),
        driveAction({{"D", logicBits(32, 0xffffffffU)}}),
        checkpointAction(
            "hold-disabled",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBits(32, 0)}},
            CheckpointKind::AfterEdge),
        driveAction({{"CLK", logicBit(false)}}),
        driveAction({{"WE", logicBit(true)}}),
        checkpointAction(
            "write-all-ones",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBits(32, 0xffffffffU)}},
            CheckpointKind::AfterEdge),
        checkpointAction(
            "no-level-capture",
            {{"D", logicBits(32, 0)}},
            {{"Q", logicBits(32, 0xffffffffU)}}),
        driveAction({{"CLK", logicBit(false)}}),
        checkpointAction(
            "write-zero",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBits(32, 0)}},
            CheckpointKind::AfterEdge),
        driveAction({{"CLK", logicBit(false)}}),
    };

    for (const auto value : {0xaaaaaaaaU, 0x55555555U}) {
        scenario.actions.push_back(
            driveAction({{"D", logicBits(32, value)}}));
        scenario.actions.push_back(checkpointAction(
            value == 0xaaaaaaaaU ? "write-aaaaaaaa" : "write-55555555",
            {{"CLK", logicBit(true)}},
            {{"Q", logicBits(32, value)}},
            CheckpointKind::AfterEdge));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(false)}}));
    }

    for (uint32_t bit_index = 0; bit_index < 32; ++bit_index) {
        const auto value = uint32_t{1} << bit_index;
        scenario.actions.push_back(
            driveAction({{"D", logicBits(32, value)}}));
        scenario.actions.push_back(checkpointAction(
            "walking-bit-" + std::to_string(bit_index),
            {{"CLK", logicBit(true)}},
            {{"Q", logicBits(32, value)}},
            CheckpointKind::AfterEdge));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(false)}}));
    }

    scenario.actions.push_back(driveAction({
        {"WE", logicBit(false)},
        {"D", logicBits(32, 0xdeadbeefU)},
    }));
    scenario.actions.push_back(checkpointAction(
        "hold-final-walking-bit",
        {{"CLK", logicBit(true)}},
        {{"Q", logicBits(32, 0x80000000U)}},
        CheckpointKind::AfterEdge));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));

    auto unknown_bit_pattern = logicBits(32, 0x12345678U);
    unknown_bit_pattern[13] = LogicValue::UNKNOWN;
    scenario.actions.push_back(driveAction({
        {"WE", logicBit(true)},
        {"D", unknown_bit_pattern},
    }));
    scenario.actions.push_back(checkpointAction(
        "capture-unknown-bit",
        {{"CLK", logicBit(true)}},
        {{"Q", unknown_bit_pattern}},
        CheckpointKind::AfterEdge));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "reset-clear",
        {{"RST", logicBit(true)}},
        {{"Q", logicBits(32, 0)}}));
    scenario.actions.push_back(checkpointAction(
        "unknown-reset-preserves-zero",
        {{"RST", logicBit(LogicValue::UNKNOWN)}},
        {{"Q", logicBits(32, 0)}},
        CheckpointKind::Settled,
        "An uncertain reset preserves zero because reset and hold agree."));
    scenario.actions.push_back(driveAction({{"RST", logicBit(false)}}));
    scenario.actions.push_back(driveAction({
        {"D", logicBits(32, 0)},
        {"WE", logicBit(LogicValue::UNKNOWN)},
    }));
    scenario.actions.push_back(checkpointAction(
        "unknown-write-enable-agrees",
        {{"CLK", logicBit(true)}},
        {{"Q", logicBits(32, 0)}},
        CheckpointKind::AfterEdge,
        "An uncertain write preserves agreed zeros in the gate-built feedback mux."));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "reset-before-high-z",
        {{"RST", logicBit(true)}},
        {{"Q", logicBits(32, 0)}}));
    auto high_z_pattern = logicBits(32, 0x12345678U);
    high_z_pattern[17] = LogicValue::HIGH_Z;
    auto normalized_high_z = high_z_pattern;
    normalized_high_z[17] = LogicValue::UNKNOWN;
    scenario.actions.push_back(driveAction({
        {"RST", logicBit(false)},
        {"WE", logicBit(true)},
        {"D", high_z_pattern},
    }));
    scenario.actions.push_back(checkpointAction(
        "capture-high-z-as-unknown",
        {{"CLK", logicBit(true)}},
        {{"Q", normalized_high_z}},
        CheckpointKind::AfterEdge,
        "Each high-impedance data lane is stored as unknown."));

    // Both X and Z control values must follow the reused FFWord mux, including
    // the previously untested agreed-one case (the AND/OR network produces X).
    for (const auto uncertain : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        const std::string label = uncertain == LogicValue::UNKNOWN ? "x" : "z";
        scenario.actions.push_back(driveAction({{"CLK", logicBit(false)}}));
        scenario.actions.push_back(driveAction({
            {"RST", logicBit(false)}, {"WE", logicBit(true)},
            {"D", logicBits(32, 0xffffffffU)},
        }));
        scenario.actions.push_back(driveAction({{"CLK", logicBit(true)}}));
        scenario.actions.push_back(driveAction({{"CLK", logicBit(false)}}));
        scenario.actions.push_back(driveAction({{"WE", logicBit(uncertain)}}));
        scenario.actions.push_back(checkpointAction(
            label + "-enable-agreed-ones", {{"CLK", logicBit(true)}},
            {{"Q", LogicVector(32, LogicValue::UNKNOWN)}}, CheckpointKind::AfterEdge));
        scenario.actions.push_back(checkpointAction(
            label + "-reset-priority-clock-high", {{"RST", logicBit(true)}},
            {{"Q", logicBits(32, 0)}}, CheckpointKind::Settled,
            "Asynchronous reset clears storage even while CLK is high and WE is uncertain."));
        scenario.actions.push_back(driveAction({{"CLK", logicBit(false)}}));
        scenario.actions.push_back(driveAction({
            {"RST", logicBit(false)}, {"D", logicBits(32, 0x00ff00ffU)},
        }));
        auto uncertain_bytes = logicBits(32, 0);
        for (size_t bit = 0; bit < 32; ++bit)
            if ((0x00ff00ffU >> bit) & 1U) uncertain_bytes[bit] = LogicValue::UNKNOWN;
        scenario.actions.push_back(checkpointAction(
            label + "-enable-mixed-data", {{"CLK", logicBit(true)}},
            {{"Q", uncertain_bytes}}, CheckpointKind::AfterEdge));
        scenario.actions.push_back(driveAction({{"CLK", logicBit(false)}}));
        scenario.actions.push_back(driveAction({
            {"WE", logicBit(true)}, {"D", logicBits(32, 0xaaaaaaaaU)},
        }));
        scenario.actions.push_back(driveAction({{"CLK", logicBit(true)}}));
        auto reset_result = logicBits(32, 0);
        for (size_t bit = 1; bit < 32; bit += 2) reset_result[bit] = LogicValue::UNKNOWN;
        scenario.actions.push_back(checkpointAction(
            label + "-reset-mixed-state", {{"RST", logicBit(uncertain)}},
            {{"Q", reset_result}}));
        scenario.actions.push_back(checkpointAction(
            label + "-reset-recovery", {{"RST", logicBit(true)}},
            {{"Q", logicBits(32, 0)}}));
    }

    return {
        "RegisterTest",
        std::string(circuit::families::registerWord(32).id()),
        "REGISTER32_ROOT",
        {},
        {std::move(scenario)},
    };
}

circuit::test::ComponentTestSpec registerFile32x32Spec() {
    using namespace circuit::test;

    const auto zero = logicBits(32, 0);
    const auto all_unknown =
        LogicVector(32, LogicValue::UNKNOWN);
    const auto alternating_high = logicBits(32, 0xaaaaaaaaU);
    const auto alternating_low = logicBits(32, 0x55555555U);
    const auto ambiguous_write_data = logicBits(32, 0x12345678U);
    const LogicVector maybe_x3_or_x7{
        LogicValue::HIGH,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::LOW,
        LogicValue::LOW,
    };
    const LogicVector maybe_x3_or_x7_high_z{
        LogicValue::HIGH,
        LogicValue::HIGH,
        LogicValue::HIGH_Z,
        LogicValue::LOW,
        LogicValue::LOW,
    };

    ActionScenario scenario{
        "contract",
        {},
        {},
        {100'000, 4'000'000},
    };
    scenario.actions = {
        checkpointAction(
            "reset",
            {
                {"RS1_ADDR", logicBits(5, 0)},
                {"RS2_ADDR", logicBits(5, 1)},
                {"RD_ADDR", logicBits(5, 0)},
                {"WRITE_DATA", zero},
                {"REG_WRITE", logicBit(false)},
                {"CLK", logicBit(false)},
                {"RST", logicBit(true)},
            },
            {
                {"RS1_DATA", zero},
                {"RS2_DATA", zero},
            }),
        driveAction({{"RST", logicBit(false)}}),
        driveAction({
            {"WRITE_DATA", logicBits(32, 0xffffffffU)},
            {"RD_ADDR", logicBits(5, 0)},
            {"REG_WRITE", logicBit(true)},
        }),
        driveAction({{"CLK", logicBit(true)}}),
        driveAction({{"CLK", logicBit(false)}}),
        checkpointAction(
            "ignore-x0-write",
            {
                {"RS1_ADDR", logicBits(5, 0)},
                {"RS2_ADDR", logicBits(5, 1)},
            },
            {
                {"RS1_DATA", zero},
                {"RS2_DATA", zero},
            }),
    };

    for (uint32_t reg = 1; reg < 32; ++reg) {
        scenario.actions.push_back(driveAction({
            {"WRITE_DATA", logicBits(32, registerFilePattern(reg))},
            {"RD_ADDR", logicBits(5, reg)},
        }));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(true)}}));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(false)}}));
        scenario.actions.push_back(checkpointAction(
            "write-x" + std::to_string(reg),
            {
                {"RS1_ADDR", logicBits(5, reg)},
                {"RS2_ADDR", logicBits(5, reg - 1)},
            },
            {
                {"RS1_DATA", logicBits(32, registerFilePattern(reg))},
                {"RS2_DATA", reg == 1
                    ? zero
                    : logicBits(32, registerFilePattern(reg - 1))},
            }));
    }

    scenario.actions.push_back(driveAction({
        {"REG_WRITE", logicBit(false)},
        {"WRITE_DATA", logicBits(32, 0xdeadbeefU)},
        {"RD_ADDR", logicBits(5, 5)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "hold-disabled",
        {
            {"RS1_ADDR", logicBits(5, 5)},
            {"RS2_ADDR", logicBits(5, 31)},
        },
        {
            {"RS1_DATA", logicBits(32, registerFilePattern(5))},
            {"RS2_DATA", logicBits(32, registerFilePattern(31))},
        }));

    scenario.actions.push_back(driveAction({
        {"REG_WRITE", logicBit(true)},
        {"WRITE_DATA", logicBits(32, 0xaaaaaaaaU)},
        {"RD_ADDR", logicBits(5, 0)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "ignore-x0-again",
        {
            {"RS1_ADDR", logicBits(5, 0)},
            {"RS2_ADDR", logicBits(5, 31)},
        },
        {
            {"RS1_DATA", zero},
            {"RS2_DATA", logicBits(32, registerFilePattern(31))},
        }));

    auto unknown_x13 = logicBits(32, 0xcafebabeU);
    unknown_x13[7] = LogicValue::UNKNOWN;
    scenario.actions.push_back(driveAction({
        {"WRITE_DATA", unknown_x13},
        {"RD_ADDR", logicBits(5, 13)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "unknown-data-lane",
        {
            {"RS1_ADDR", logicBits(5, 13)},
            {"RS2_ADDR", logicBits(5, 12)},
        },
        {
            {"RS1_DATA", unknown_x13},
            {"RS2_DATA", logicBits(32, registerFilePattern(12))},
        }));

    auto high_z_x14_input = logicBits(32, 0x13579bdfU);
    high_z_x14_input[9] = LogicValue::HIGH_Z;
    auto high_z_x14_expected = high_z_x14_input;
    high_z_x14_expected[9] = LogicValue::UNKNOWN;
    scenario.actions.push_back(driveAction({
        {"WRITE_DATA", high_z_x14_input},
        {"RD_ADDR", logicBits(5, 14)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "high-z-data-lane-normalizes-to-unknown",
        {
            {"RS1_ADDR", logicBits(5, 14)},
            {"RS2_ADDR", logicBits(5, 0)},
        },
        {
            {"RS1_DATA", high_z_x14_expected},
            {"RS2_DATA", zero},
        }));

    // Reset gives the unknown-address policy a clean, known starting point.
    scenario.actions.push_back(checkpointAction(
        "reset-before-unknown-policy",
        {
            {"RST", logicBit(true)},
            {"RS1_ADDR", logicBits(5, 3)},
            {"RS2_ADDR", logicBits(5, 7)},
        },
        {
            {"RS1_DATA", zero},
            {"RS2_DATA", zero},
        }));
    scenario.actions.push_back(driveAction({{"RST", logicBit(false)}}));

    const auto appendWrite = [&](LogicVector address, LogicVector data) {
        scenario.actions.push_back(driveAction({
            {"RD_ADDR", std::move(address)},
            {"WRITE_DATA", std::move(data)},
            {"REG_WRITE", logicBit(true)},
        }));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(true)}}));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(false)}}));
    };

    appendWrite(logicBits(5, 3), logicBits(32, 0xaaaaaaaaU));
    scenario.actions.push_back(checkpointAction(
        "write-x3-for-unknown-policy",
        {{"RS1_ADDR", logicBits(5, 3)}},
        {{"RS1_DATA", logicBits(32, 0xaaaaaaaaU)}}));
    appendWrite(logicBits(5, 7), logicBits(32, 0xaaaaaaaaU));
    scenario.actions.push_back(checkpointAction(
        "ambiguous-read-agrees",
        {{"RS1_ADDR", maybe_x3_or_x7}},
        {{"RS1_DATA", gateUnknownSelect(alternating_high)}}));
    scenario.actions.push_back(checkpointAction(
        "high-z-read-address-matches-unknown-policy",
        {{"RS1_ADDR", maybe_x3_or_x7_high_z}},
        {{"RS1_DATA", gateUnknownSelect(alternating_high)}}));
    appendWrite(logicBits(5, 7), logicBits(32, 0x55555555U));
    scenario.actions.push_back(checkpointAction(
        "ambiguous-read-differs",
        {{"RS1_ADDR", maybe_x3_or_x7}},
        {{"RS1_DATA", all_unknown}}));
    appendWrite(maybe_x3_or_x7, ambiguous_write_data);
    scenario.actions.push_back(checkpointAction(
        "ambiguous-write",
        {
            {"RS1_ADDR", logicBits(5, 3)},
            {"RS2_ADDR", logicBits(5, 7)},
        },
        {
            {"RS1_DATA", gateUnknownWrite(
                alternating_high, ambiguous_write_data)},
            {"RS2_DATA", gateUnknownWrite(
                alternating_low, ambiguous_write_data)},
        }));

    scenario.actions.push_back(checkpointAction(
        "reset-clears-ambiguous-write",
        {{"RST", logicBit(true)}},
        {
            {"RS1_DATA", zero},
            {"RS2_DATA", zero},
        }));
    scenario.actions.push_back(driveAction({{"RST", logicBit(false)}}));
    scenario.actions.push_back(driveAction({
        {"RD_ADDR", logicBits(5, 9)},
        {"WRITE_DATA", logicBits(32, 0xffffffffU)},
        {"REG_WRITE", logicBit(LogicValue::UNKNOWN)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "unknown-register-write",
        {
            {"RS1_ADDR", logicBits(5, 9)},
            {"RS2_ADDR", logicBits(5, 0)},
        },
        {
            {"RS1_DATA", all_unknown},
            {"RS2_DATA", zero},
        }));
    scenario.actions.push_back(checkpointAction(
        "unknown-reset",
        {
            {"RST", logicBit(LogicValue::UNKNOWN)},
            {"RS1_ADDR", logicBits(5, 1)},
            {"RS2_ADDR", logicBits(5, 0)},
        },
        {
            {"RS1_DATA", zero},
            {"RS2_DATA", zero},
        }));
    scenario.actions.push_back(checkpointAction(
        "high-z-reset-matches-unknown-policy",
        {{"RST", logicBit(LogicValue::HIGH_Z)}},
        {
            {"RS1_DATA", zero},
            {"RS2_DATA", zero},
        }));

    return {
        "RegisterFile32x32Test",
        std::string(circuit::families::RegisterFile32x32.id()),
        "REGISTER_FILE32X32_ROOT",
        {},
        {std::move(scenario)},
    };
}

} // namespace

FlipFlopCellTest::FlipFlopCellTest()
    : circuit::test::ComponentScenarioTest(
          flipFlopCellSpec(), "contract") {}

RegisterTest::RegisterTest()
    : circuit::test::ComponentScenarioTest(
          register32Spec(), "contract") {}

RegisterFile32x32Test::RegisterFile32x32Test()
    : circuit::test::ComponentScenarioTest(
          registerFile32x32Spec(), "contract") {}
