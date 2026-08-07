#include "tests/MemoryComponentTests.hpp"

#include "components/Component.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
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
void requireMemory(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

uint32_t registerFilePattern(uint32_t reg_index) {
    return 0x10000000U | (reg_index * 0x01010101U);
}

uint32_t memoryWordPattern(uint32_t word_index) {
    return 0x20000000U | (word_index * 0x01020304U);
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

circuit::test::ComponentTestSpec memoryBitSpec() {
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
        "MemoryBitTest",
        std::string(circuit::families::MemoryBit.id()),
        "MEMORY_BIT_ROOT",
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
        "An uncertain write preserves bits where hold and write agree."));
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

    return {
        "Register32Test",
        std::string(circuit::families::Register32.id()),
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

circuit::test::NamedValues memoryOutputs(
    uint32_t read_data,
    bool fault) {
    using namespace circuit::test;
    return {
        {"READ_DATA", logicBits(32, read_data)},
        {"READY", logicBit(true)},
        {"FAULT", logicBit(fault)},
    };
}

circuit::test::NamedValues memoryOutputs(
    circuit::test::LogicVector read_data,
    bool fault) {
    using namespace circuit::test;
    return {
        {"READ_DATA", std::move(read_data)},
        {"READY", logicBit(true)},
        {"FAULT", logicBit(fault)},
    };
}

void appendMemoryWrite(
    circuit::test::ActionScenario& scenario,
    uint32_t address,
    circuit::test::LogicVector value,
    uint32_t size) {
    using namespace circuit::test;
    scenario.actions.push_back(driveAction({
        {"ADDR", logicBits(32, address)},
        {"WRITE_DATA", std::move(value)},
        {"READ_EN", logicBit(false)},
        {"WRITE_EN", logicBit(true)},
        {"SIZE", logicBits(2, size)},
        {"SIGN_EXTEND", logicBit(false)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(
        driveAction({{"WRITE_EN", logicBit(false)}}));
}

void appendMemoryWrite(
    circuit::test::ActionScenario& scenario,
    uint32_t address,
    uint32_t value,
    uint32_t size) {
    appendMemoryWrite(
        scenario,
        address,
        circuit::test::logicBits(32, value),
        size);
}

circuit::test::ComponentTestSpec registerFile4x32Spec() {
    using namespace circuit::test;
    const auto zero = logicBits(32, 0);

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
                {"RS1_ADDR", logicBits(2, 0)},
                {"RS2_ADDR", logicBits(2, 1)},
                {"RD_ADDR", logicBits(2, 0)},
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
    };

    const auto appendWriteAddress = [&](
        LogicVector address,
        LogicVector value) {
        scenario.actions.push_back(driveAction({
            {"RD_ADDR", std::move(address)},
            {"WRITE_DATA", std::move(value)},
            {"REG_WRITE", logicBit(true)},
        }));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(true)}}));
        scenario.actions.push_back(
            driveAction({{"CLK", logicBit(false)}}));
    };
    const auto appendWrite = [&](
        uint32_t address,
        LogicVector value) {
        appendWriteAddress(
            logicBits(2, address),
            std::move(value));
    };

    appendWrite(0, logicBits(32, 0xffffffffU));
    scenario.actions.push_back(checkpointAction(
        "ignore-x0-write",
        {
            {"RS1_ADDR", logicBits(2, 0)},
            {"RS2_ADDR", logicBits(2, 1)},
        },
        {
            {"RS1_DATA", zero},
            {"RS2_DATA", zero},
        }));

    for (uint32_t reg = 1; reg < 4; ++reg) {
        const auto value = reg * 0x11111111U;
        appendWrite(reg, logicBits(32, value));
        scenario.actions.push_back(checkpointAction(
            "write-x" + std::to_string(reg),
            {
                {"RS1_ADDR", logicBits(2, reg)},
                {"RS2_ADDR", logicBits(2, reg - 1)},
            },
            {
                {"RS1_DATA", logicBits(32, value)},
                {"RS2_DATA", reg == 1
                    ? zero
                    : logicBits(32, (reg - 1) * 0x11111111U)},
            }));
    }

    scenario.actions.push_back(checkpointAction(
        "independent-read-ports",
        {
            {"RS1_ADDR", logicBits(2, 1)},
            {"RS2_ADDR", logicBits(2, 3)},
        },
        {
            {"RS1_DATA", logicBits(32, 0x11111111U)},
            {"RS2_DATA", logicBits(32, 0x33333333U)},
        }));

    scenario.actions.push_back(driveAction({
        {"RD_ADDR", logicBits(2, 1)},
        {"WRITE_DATA", logicBits(32, 0xaaaaaaaaU)},
        {"REG_WRITE", logicBit(false)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "hold-disabled",
        {
            {"RS1_ADDR", logicBits(2, 1)},
            {"RS2_ADDR", logicBits(2, 2)},
        },
        {
            {"RS1_DATA", logicBits(32, 0x11111111U)},
            {"RS2_DATA", logicBits(32, 0x22222222U)},
        }));

    appendWrite(2, logicBits(32, 0x12345678U));
    scenario.actions.push_back(checkpointAction(
        "overwrite-x2",
        {
            {"RS1_ADDR", logicBits(2, 2)},
            {"RS2_ADDR", logicBits(2, 1)},
        },
        {
            {"RS1_DATA", logicBits(32, 0x12345678U)},
            {"RS2_DATA", logicBits(32, 0x11111111U)},
        }));

    auto unknown_x3 = logicBits(32, 0xcafebabeU);
    unknown_x3[7] = LogicValue::UNKNOWN;
    appendWrite(3, unknown_x3);
    scenario.actions.push_back(checkpointAction(
        "unknown-data-lane",
        {
            {"RS1_ADDR", logicBits(2, 3)},
            {"RS2_ADDR", logicBits(2, 0)},
        },
        {
            {"RS1_DATA", unknown_x3},
            {"RS2_DATA", zero},
        }));

    scenario.actions.push_back(checkpointAction(
        "reset-clear",
        {
            {"RST", logicBit(true)},
            {"RS1_ADDR", logicBits(2, 1)},
            {"RS2_ADDR", logicBits(2, 3)},
        },
        {
            {"RS1_DATA", zero},
            {"RS2_DATA", zero},
        }));

    scenario.actions.push_back(driveAction({{"RST", logicBit(false)}}));
    appendWrite(1, logicBits(32, 0xaaaaaaaaU));
    appendWrite(3, logicBits(32, 0xaaaaaaaaU));
    const LogicVector maybe_x1_or_x3_high_z{
        LogicValue::HIGH,
        LogicValue::HIGH_Z,
    };
    scenario.actions.push_back(checkpointAction(
        "high-z-read-address",
        {
            {"RS1_ADDR", maybe_x1_or_x3_high_z},
            {"RS2_ADDR", logicBits(2, 0)},
        },
        {
            {"RS1_DATA", gateUnknownSelect(
                logicBits(32, 0xaaaaaaaaU))},
            {"RS2_DATA", zero},
        }));

    const auto ambiguous_data = logicBits(32, 0x12345678U);
    appendWriteAddress(maybe_x1_or_x3_high_z, ambiguous_data);
    const auto ambiguous_result = gateUnknownWrite(
        logicBits(32, 0xaaaaaaaaU), ambiguous_data);
    scenario.actions.push_back(checkpointAction(
        "high-z-write-address",
        {
            {"RS1_ADDR", logicBits(2, 1)},
            {"RS2_ADDR", logicBits(2, 3)},
        },
        {
            {"RS1_DATA", ambiguous_result},
            {"RS2_DATA", ambiguous_result},
        }));

    auto high_z_x2_input = logicBits(32, 0x13579bdfU);
    high_z_x2_input[9] = LogicValue::HIGH_Z;
    auto high_z_x2_expected = high_z_x2_input;
    high_z_x2_expected[9] = LogicValue::UNKNOWN;
    appendWrite(2, high_z_x2_input);
    scenario.actions.push_back(checkpointAction(
        "high-z-data-lane-normalizes-to-unknown",
        {
            {"RS1_ADDR", logicBits(2, 2)},
            {"RS2_ADDR", logicBits(2, 0)},
        },
        {
            {"RS1_DATA", high_z_x2_expected},
            {"RS2_DATA", zero},
        }));

    return {
        "RegisterFile4x32Test",
        "memory.register-file.4x32",
        "REGISTER_FILE4X32_ROOT",
        {},
        {std::move(scenario)},
    };
}

circuit::test::ComponentTestSpec memorySliceSpec(
    std::string test_id,
    std::string contract_id,
    uint32_t word_count) {
    using namespace circuit::test;

    ActionScenario scenario{
        "contract",
        {},
        {},
        {250'000, 10'000'000},
    };
    scenario.actions = {
        checkpointAction(
            "reset-read",
            {
                {"ADDR", logicBits(32, 0)},
                {"WRITE_DATA", logicBits(32, 0)},
                {"READ_EN", logicBit(true)},
                {"WRITE_EN", logicBit(false)},
                {"SIZE", logicBits(2, 2)},
                {"SIGN_EXTEND", logicBit(false)},
                {"CLK", logicBit(false)},
                {"RST", logicBit(true)},
            },
            memoryOutputs(0, false),
            CheckpointKind::Settled,
            "Reset clears every word."),
        driveAction({{"RST", logicBit(false)}}),
    };

    for (uint32_t word = 0; word < word_count; ++word) {
        const auto value = memoryWordPattern(word);
        appendMemoryWrite(scenario, word * 4U, value, 2);
        scenario.actions.push_back(checkpointAction(
            "write-word-" + std::to_string(word),
            {
                {"ADDR", logicBits(32, word * 4U)},
                {"READ_EN", logicBit(true)},
            },
            memoryOutputs(value, false),
            CheckpointKind::TransactionComplete,
            "An aligned word store is visible through public readback."));
    }

    scenario.actions.push_back(driveAction({
        {"ADDR", logicBits(32, 4)},
        {"WRITE_DATA", logicBits(32, 0xdeadbeefU)},
        {"READ_EN", logicBit(false)},
        {"WRITE_EN", logicBit(false)},
        {"SIZE", logicBits(2, 2)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "write-disabled",
        {{"READ_EN", logicBit(true)}},
        memoryOutputs(memoryWordPattern(1), false),
        CheckpointKind::AfterEdge,
        "WRITE_EN=0 preserves the selected word."));

    scenario.actions.push_back(checkpointAction(
        "unsupported-byte",
        {
            {"ADDR", logicBits(32, 0)},
            {"SIZE", logicBits(2, 0)},
        },
        memoryOutputs(memoryWordPattern(0), true)));
    scenario.actions.push_back(checkpointAction(
        "unsupported-halfword",
        {{"SIZE", logicBits(2, 1)}},
        memoryOutputs(memoryWordPattern(0), true)));
    scenario.actions.push_back(checkpointAction(
        "misaligned-word",
        {
            {"ADDR", logicBits(32, 2)},
            {"SIZE", logicBits(2, 2)},
        },
        memoryOutputs(memoryWordPattern(0), true)));
    scenario.actions.push_back(checkpointAction(
        "out-of-range",
        {{"ADDR", logicBits(32, word_count * 4U)}},
        memoryOutputs(memoryWordPattern(0), true)));
    scenario.actions.push_back(checkpointAction(
        "no-access",
        {
            {"READ_EN", logicBit(false)},
            {"WRITE_EN", logicBit(false)},
        },
        memoryOutputs(memoryWordPattern(0), false)));

    scenario.actions.push_back(driveAction({
        {"ADDR", logicBits(32, 2)},
        {"WRITE_DATA", logicBits(32, 0xffffffffU)},
        {"READ_EN", logicBit(false)},
        {"WRITE_EN", logicBit(true)},
        {"SIZE", logicBits(2, 2)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "faulted-write-blocked",
        {
            {"ADDR", logicBits(32, 0)},
            {"READ_EN", logicBit(true)},
            {"WRITE_EN", logicBit(false)},
        },
        memoryOutputs(memoryWordPattern(0), false),
        CheckpointKind::TransactionComplete,
        "A misaligned write never changes storage."));

    auto four_state_word = logicBits(32, 0x12345678U);
    four_state_word[5] = LogicValue::UNKNOWN;
    four_state_word[17] = LogicValue::HIGH_Z;
    auto normalized_four_state_word = four_state_word;
    normalized_four_state_word[17] = LogicValue::UNKNOWN;
    appendMemoryWrite(scenario, 0, four_state_word, 2);
    scenario.actions.push_back(checkpointAction(
        "four-state-word",
        {
            {"ADDR", logicBits(32, 0)},
            {"READ_EN", logicBit(true)},
        },
        memoryOutputs(normalized_four_state_word, false),
        CheckpointKind::TransactionComplete,
        "Stored HIGH_Z data is normalized to UNKNOWN while known lanes remain intact."));

    scenario.actions.push_back(checkpointAction(
        "reset-clear",
        {
            {"RST", logicBit(true)},
            {"ADDR", logicBits(32, (word_count - 1U) * 4U)},
        },
        memoryOutputs(0, false)));

    return {
        std::move(test_id),
        std::move(contract_id),
        word_count == 4
            ? "MEMORY4X32_ROOT"
            : "MEMORY32X32_ROOT",
        {},
        {std::move(scenario)},
    };
}

circuit::test::ComponentTestSpec memory64Kx32Spec() {
    using namespace circuit::test;
    constexpr uint32_t LastWordAddress = 0x0003fffcU;
    constexpr uint32_t FirstOutOfRangeAddress = 0x00040000U;

    ActionScenario scenario{
        "contract",
        {},
        {},
        {50'000, 2'000'000},
    };
    scenario.actions = {
        checkpointAction(
            "reset-read",
            {
                {"ADDR", logicBits(32, 0)},
                {"WRITE_DATA", logicBits(32, 0)},
                {"READ_EN", logicBit(true)},
                {"WRITE_EN", logicBit(false)},
                {"SIZE", logicBits(2, 2)},
                {"SIGN_EXTEND", logicBit(false)},
                {"CLK", logicBit(false)},
                {"RST", logicBit(true)},
            },
            memoryOutputs(0, false)),
        driveAction({{"RST", logicBit(false)}}),
    };

    appendMemoryWrite(scenario, 0, 0x12345678U, 2);
    scenario.actions.push_back(checkpointAction(
        "word-base",
        {
            {"ADDR", logicBits(32, 0)},
            {"READ_EN", logicBit(true)},
        },
        memoryOutputs(0x12345678U, false),
        CheckpointKind::TransactionComplete));

    appendMemoryWrite(
        scenario, LastWordAddress, 0x89abcdefU, 2);
    scenario.actions.push_back(checkpointAction(
        "word-limit",
        {
            {"ADDR", logicBits(32, LastWordAddress)},
            {"READ_EN", logicBit(true)},
        },
        memoryOutputs(0x89abcdefU, false),
        CheckpointKind::TransactionComplete));

    appendMemoryWrite(scenario, 4, 0x01020304U, 2);
    scenario.actions.push_back(checkpointAction(
        "word-one",
        {
            {"ADDR", logicBits(32, 4)},
            {"READ_EN", logicBit(true)},
        },
        memoryOutputs(0x01020304U, false),
        CheckpointKind::TransactionComplete));

    scenario.actions.push_back(driveAction({
        {"ADDR", logicBits(32, 4)},
        {"WRITE_DATA", logicBits(32, 0xdeadbeefU)},
        {"READ_EN", logicBit(false)},
        {"WRITE_EN", logicBit(false)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "write-disabled",
        {{"READ_EN", logicBit(true)}},
        memoryOutputs(0x01020304U, false),
        CheckpointKind::AfterEdge));

    appendMemoryWrite(scenario, 1, 0x000000aaU, 0);
    scenario.actions.push_back(checkpointAction(
        "byte-store",
        {
            {"ADDR", logicBits(32, 0)},
            {"READ_EN", logicBit(true)},
            {"SIZE", logicBits(2, 2)},
        },
        memoryOutputs(0x1234aa78U, false),
        CheckpointKind::TransactionComplete));

    appendMemoryWrite(scenario, 2, 0x0000beefU, 1);
    scenario.actions.push_back(checkpointAction(
        "halfword-store",
        {
            {"ADDR", logicBits(32, 0)},
            {"READ_EN", logicBit(true)},
            {"SIZE", logicBits(2, 2)},
        },
        memoryOutputs(0xbeefaa78U, false),
        CheckpointKind::TransactionComplete));

    appendMemoryWrite(scenario, 8, 0, 2);
    scenario.actions.push_back(checkpointAction(
        "zero-word-store",
        {
            {"ADDR", logicBits(32, 8)},
            {"READ_EN", logicBit(true)},
        },
        memoryOutputs(0, false),
        CheckpointKind::TransactionComplete));

    scenario.actions.push_back(checkpointAction(
        "read-byte-unsigned",
        {
            {"ADDR", logicBits(32, 2)},
            {"SIZE", logicBits(2, 0)},
            {"SIGN_EXTEND", logicBit(false)},
        },
        memoryOutputs(0x000000efU, false)));
    scenario.actions.push_back(checkpointAction(
        "read-byte-signed",
        {{"SIGN_EXTEND", logicBit(true)}},
        memoryOutputs(0xffffffefU, false)));
    scenario.actions.push_back(checkpointAction(
        "read-halfword-unsigned",
        {
            {"SIZE", logicBits(2, 1)},
            {"SIGN_EXTEND", logicBit(false)},
        },
        memoryOutputs(0x0000beefU, false)));
    scenario.actions.push_back(checkpointAction(
        "read-halfword-signed",
        {{"SIGN_EXTEND", logicBit(true)}},
        memoryOutputs(0xffffbeefU, false)));

    auto four_state_word = logicBits(32, 0x12345678U);
    four_state_word[5] = LogicValue::UNKNOWN;
    four_state_word[17] = LogicValue::HIGH_Z;
    auto normalized_four_state_word = four_state_word;
    normalized_four_state_word[17] = LogicValue::UNKNOWN;
    appendMemoryWrite(scenario, 8, four_state_word, 2);
    scenario.actions.push_back(checkpointAction(
        "four-state-word",
        {
            {"ADDR", logicBits(32, 8)},
            {"READ_EN", logicBit(true)},
        },
        memoryOutputs(normalized_four_state_word, false),
        CheckpointKind::TransactionComplete));

    auto unknown_address = logicBits(32, 0);
    unknown_address[3] = LogicValue::UNKNOWN;
    scenario.actions.push_back(checkpointAction(
        "unknown-address",
        {
            {"ADDR", unknown_address},
            {"SIZE", logicBits(2, 2)},
        },
        {
            {"READ_DATA", LogicVector(32, LogicValue::UNKNOWN)},
            {"READY", logicBit(true)},
            {"FAULT", logicBit(LogicValue::UNKNOWN)},
        }));
    scenario.actions.push_back(checkpointAction(
        "unknown-read-enable",
        {
            {"ADDR", logicBits(32, 8)},
            {"READ_EN", logicBit(LogicValue::UNKNOWN)},
        },
        {
            {"READ_DATA", normalized_four_state_word},
            {"READY", logicBit(true)},
            {"FAULT", logicBit(LogicValue::UNKNOWN)},
        }));

    scenario.actions.push_back(checkpointAction(
        "invalid-size",
        {
            {"ADDR", logicBits(32, 0)},
            {"READ_EN", logicBit(true)},
            {"SIZE", logicBits(2, 3)},
            {"SIGN_EXTEND", logicBit(false)},
        },
        memoryOutputs(
            LogicVector(32, LogicValue::UNKNOWN), true)));
    scenario.actions.push_back(checkpointAction(
        "misaligned-halfword",
        {
            {"ADDR", logicBits(32, 1)},
            {"SIZE", logicBits(2, 1)},
        },
        memoryOutputs(
            LogicVector(32, LogicValue::UNKNOWN), true)));
    scenario.actions.push_back(checkpointAction(
        "misaligned-word",
        {
            {"ADDR", logicBits(32, 2)},
            {"SIZE", logicBits(2, 2)},
        },
        memoryOutputs(
            LogicVector(32, LogicValue::UNKNOWN), true)));
    scenario.actions.push_back(checkpointAction(
        "out-of-range",
        {
            {"ADDR", logicBits(32, FirstOutOfRangeAddress)},
            {"SIZE", logicBits(2, 0)},
        },
        memoryOutputs(
            LogicVector(32, LogicValue::UNKNOWN), true)));
    scenario.actions.push_back(checkpointAction(
        "no-access",
        {
            {"READ_EN", logicBit(false)},
            {"WRITE_EN", logicBit(false)},
        },
        memoryOutputs(
            LogicVector(32, LogicValue::UNKNOWN), false)));

    scenario.actions.push_back(driveAction({
        {"ADDR", logicBits(32, 2)},
        {"WRITE_DATA", logicBits(32, 0xffffffffU)},
        {"READ_EN", logicBit(false)},
        {"WRITE_EN", logicBit(true)},
        {"SIZE", logicBits(2, 2)},
    }));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(true)}}));
    scenario.actions.push_back(
        driveAction({{"CLK", logicBit(false)}}));
    scenario.actions.push_back(checkpointAction(
        "faulted-write-blocked",
        {
            {"ADDR", logicBits(32, 0)},
            {"READ_EN", logicBit(true)},
            {"WRITE_EN", logicBit(false)},
        },
        memoryOutputs(0xbeefaa78U, false),
        CheckpointKind::TransactionComplete));

    scenario.actions.push_back(checkpointAction(
        "reset-clear",
        {
            {"RST", logicBit(true)},
            {"ADDR", logicBits(32, LastWordAddress)},
            {"SIZE", logicBits(2, 2)},
        },
        memoryOutputs(0, false)));

    return {
        "Memory64Kx32Test",
        std::string(circuit::families::Memory64Kx32.id()),
        "MEMORY64KX32_ROOT",
        {},
        {std::move(scenario)},
    };
}
}

MemoryBitTest::MemoryBitTest()
    : circuit::test::ComponentScenarioTest(
          memoryBitSpec(), "contract") {}

Memory64Kx32Test::Memory64Kx32Test()
    : circuit::test::ComponentScenarioTest(
          memory64Kx32Spec(), "contract") {}

bool Memory64Kx32Test::run() {
    if (!circuit::test::ComponentScenarioTest::run()) {
        return false;
    }

    try {
        const auto& artifacts = getRunArtifacts();
        requireMemory(
            artifacts.size() == 1,
            "Memory64Kx32 has exactly one selectable implementation");
        const auto memory =
            std::dynamic_pointer_cast<Memory64Kx32>(
                artifacts.front().root);
        requireMemory(
            memory != nullptr,
            "Memory64Kx32 artifact has the wrong root type");

        std::map<std::string, size_t> times;
        for (const auto& checkpoint :
             artifacts.front().checkpoints) {
            times.emplace(checkpoint.id, checkpoint.actual_time);
        }
        const auto timeOf = [&](const std::string& id) {
            const auto found = times.find(id);
            if (found == times.end()) {
                throw std::runtime_error(
                    "Missing memory checkpoint '" + id + "'");
            }
            return found->second;
        };
        const auto requireWord = [&](
            const std::string& checkpoint_id,
            uint32_t address,
            uint32_t expected) {
            const auto words = memory->getWordsAtTime(
                timeOf(checkpoint_id), address, 1);
            requireMemory(
                words.size() == 1
                    && words.front().first
                        == (address & ~uint32_t{0x3})
                    && words.front().second
                        == circuit::test::logicBits(32, expected),
                checkpoint_id
                    + ": historical memory word mismatch");
        };

        requireWord("word-base", 0, 0x12345678U);
        requireWord("word-limit", 0x0003fffcU, 0x89abcdefU);
        requireWord("halfword-store", 0, 0xbeefaa78U);
        requireWord("zero-word-store", 8, 0);
        requireWord("reset-clear", 0x0003fffcU, 0);

        const auto base_writes =
            memory->getByteWritesInTimeRange(
                timeOf("reset-read"), timeOf("word-base"));
        requireMemory(
            base_writes.size() == 4
                && base_writes.at(0) == 0x78
                && base_writes.at(1) == 0x56
                && base_writes.at(2) == 0x34
                && base_writes.at(3) == 0x12,
            "Word bus-write history lost little-endian byte lanes");

        const auto zero_writes =
            memory->getByteWritesInTimeRange(
                timeOf("halfword-store"),
                timeOf("zero-word-store"));
        requireMemory(
            zero_writes.size() == 4
                && zero_writes.at(8) == 0
                && zero_writes.at(9) == 0
                && zero_writes.at(10) == 0
                && zero_writes.at(11) == 0,
            "Zero-valued stores must remain visible in write history");

        requireMemory(
            memory->getByteWritesInTimeRange(
                timeOf("no-access"),
                timeOf("faulted-write-blocked")).empty(),
            "A faulted write appeared in successful write history");
        requireMemory(
            memory->getTouchedWordCountAtTime(
                timeOf("faulted-write-blocked")) == 4,
            "Touched-word history omitted a successful write");
        const auto touched = memory->getTouchedWordsAtTime(
            timeOf("faulted-write-blocked"),
            Memory64Kx32::capacityWords());
        requireMemory(
            touched.size() == 4
                && touched[0].first == 0
                && touched[1].first == 4
                && touched[2].first == 8
                && touched[3].first == 0x0003fffcU,
            "Touched words are incomplete or not address-sorted");
        requireMemory(
            memory->getTouchedWordCountAtTime(
                timeOf("reset-clear")) == 0,
            "Reset did not clear the historical touched-word view");

        memory->setHistoryRecordingEnabled(false);
        memory->writeU32AtTime(
            timeOf("reset-clear") + 1,
            0,
            0x13579bdfU);
        requireMemory(
            memory->readWord(0) == 0x13579bdfU,
            "Headless memory mode changed functional storage");
        bool rejected_history_query = false;
        try {
            (void)memory->getByteWritesInTimeRange(
                0, timeOf("reset-clear") + 1);
        } catch (const std::logic_error&) {
            rejected_history_query = true;
        }
        requireMemory(
            rejected_history_query,
            "Headless memory mode silently returned incomplete history");
        return true;
    } catch (const std::exception& error) {
        std::cerr
            << "[FAIL] Test 'Memory64Kx32Test' historical observation: "
            << error.what() << std::endl;
        return false;
    }
}

Register32Test::Register32Test()
    : circuit::test::ComponentScenarioTest(
          register32Spec(), "contract") {}

RegisterFile4x32Test::RegisterFile4x32Test()
    : circuit::test::ComponentScenarioTest(
          registerFile4x32Spec(), "contract") {}

RegisterFile32x32Test::RegisterFile32x32Test()
    : circuit::test::ComponentScenarioTest(
          registerFile32x32Spec(), "contract") {}

Memory4x32Test::Memory4x32Test()
    : circuit::test::ComponentScenarioTest(
          memorySliceSpec(
              "Memory4x32Test",
              "memory.word-array.4x32",
              4),
          "contract") {}

Memory32x32Test::Memory32x32Test()
    : circuit::test::ComponentScenarioTest(
          memorySliceSpec(
              "Memory32x32Test",
              "memory.word-array.32x32",
              32),
          "contract") {}
