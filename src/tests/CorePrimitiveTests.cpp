#include "tests/CorePrimitiveTests.hpp"

#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "simulator/Event.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {
void expect(bool condition, const char* test_name) {
    if (!condition) {
        std::cerr << test_name << " failed" << std::endl;
        assert(false && "Core primitive test failed");
    }
}

TestValue logic(LogicValue value) {
    return TestValue(value);
}

enum class BinaryGateOperation {
    And,
    Or,
    Xor,
    Nand,
    Nor,
};

LogicValue invertKnown(LogicValue value) {
    if (value == LogicValue::LOW) {
        return LogicValue::HIGH;
    }
    if (value == LogicValue::HIGH) {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

LogicValue binaryGateExpected(
    BinaryGateOperation operation,
    LogicValue left,
    LogicValue right) {
    LogicValue result = LogicValue::UNKNOWN;
    switch (operation) {
        case BinaryGateOperation::And:
        case BinaryGateOperation::Nand:
            if (left == LogicValue::LOW || right == LogicValue::LOW) {
                result = LogicValue::LOW;
            } else if (
                left == LogicValue::HIGH
                && right == LogicValue::HIGH) {
                result = LogicValue::HIGH;
            }
            if (operation == BinaryGateOperation::Nand) {
                result = invertKnown(result);
            }
            return result;
        case BinaryGateOperation::Or:
        case BinaryGateOperation::Nor:
            if (left == LogicValue::HIGH || right == LogicValue::HIGH) {
                result = LogicValue::HIGH;
            } else if (
                left == LogicValue::LOW
                && right == LogicValue::LOW) {
                result = LogicValue::LOW;
            }
            if (operation == BinaryGateOperation::Nor) {
                result = invertKnown(result);
            }
            return result;
        case BinaryGateOperation::Xor:
            if ((left == LogicValue::LOW || left == LogicValue::HIGH)
                && (right == LogicValue::LOW
                    || right == LogicValue::HIGH)) {
                return left == right
                    ? LogicValue::LOW
                    : LogicValue::HIGH;
            }
            return LogicValue::UNKNOWN;
    }
    return LogicValue::UNKNOWN;
}

std::vector<TestRow> binaryGateRows(BinaryGateOperation operation) {
    constexpr std::array<LogicValue, 4> Values{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
    };
    std::vector<TestRow> rows;
    rows.reserve(Values.size() * Values.size());
    for (const auto left : Values) {
        for (const auto right : Values) {
            rows.push_back({
                {{"A", logic(left)}, {"B", logic(right)}},
                {{"OUT", logic(binaryGateExpected(
                    operation, left, right))}},
            });
        }
    }
    return rows;
}

template <typename Fn>
void expectOutOfRange(Fn&& fn, const char* test_name) {
    try {
        fn();
    } catch (const std::out_of_range&) {
        return;
    }
    std::cerr << test_name << " failed: expected std::out_of_range" << std::endl;
    assert(false && "Expected std::out_of_range");
}

void expectWireAt(Simulator& sim,
                  const std::shared_ptr<Wire<>>& wire,
                  size_t time,
                  LogicValue expected,
                  const char* wire_name) {
    assert(wire && "Missing wire for timestamp verification");
    sim.setCircuitStateAtTime(time);
    auto actual = wire->getSingleValue();
    if (actual != expected) {
        std::cerr << "Verification failed at t=" << time
                  << " for " << wire_name
                  << ": expected " << expected
                  << ", got " << actual << std::endl;
        assert(false && "Timestamp wire value mismatch");
    }
}

void drive(Simulator& sim, size_t time, const std::shared_ptr<Wire<>>& wire, LogicValue value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time, wire, value));
}

void drive(Simulator& sim, size_t time, const std::shared_ptr<Wire<>>& wire, bool value) {
    drive(sim, time, wire, value ? LogicValue::HIGH : LogicValue::LOW);
}

void expectSplitterOutputs(const std::shared_ptr<IOComponent>& io_root,
                           uint64_t expected,
                           const char* test_name) {
    assert(io_root && "Missing splitter root");
    for (size_t i = 0; i < 8; ++i) {
        const auto pin_name = "OUT_" + std::to_string(i);
        auto pin = io_root->getOutputPin(pin_name);
        assert(pin && "Missing splitter output pin");

        const auto expected_value = ((expected >> i) & 1U) != 0
            ? LogicValue::HIGH
            : LogicValue::LOW;
        const auto actual = pin->getValue();
        if (actual != expected_value) {
            std::cerr << test_name << " failed for " << pin_name
                      << ": expected " << expected_value
                      << ", got " << actual << std::endl;
            assert(false && "Splitter output mismatch");
        }
    }
}
}

std::string WireTemplateTest::getTestName() const {
    return "WireTemplateTest";
}

void WireTemplateTest::verifyResults() {
    Wire<8> bus("BUS");
    bus.setValue(0xA5);
    expect(bus.getValue() == 0xA5, "WireTemplateTest bus value");
    expect(bus.getBit(0) == LogicValue::HIGH, "WireTemplateTest bus bit 0");
    expect(bus.getBit(1) == LogicValue::LOW, "WireTemplateTest bus bit 1");
    expect(bus.getBit(7) == LogicValue::HIGH, "WireTemplateTest bus bit 7");

    bus.setValueVector({LogicValue::LOW, LogicValue::HIGH, LogicValue::UNKNOWN});
    expect(bus.getBit(0) == LogicValue::LOW, "WireTemplateTest vector bit 0");
    expect(bus.getBit(1) == LogicValue::HIGH, "WireTemplateTest vector bit 1");
    expect(bus.getBit(2) == LogicValue::UNKNOWN, "WireTemplateTest vector bit 2");
    expect(bus.getBit(3) == LogicValue::UNKNOWN, "WireTemplateTest vector short fill");
    bus.setBit(7, LogicValue::HIGH);
    expect(bus.getBit(7) == LogicValue::HIGH, "WireTemplateTest setBit high bit");
    expect(bus.getValue() == 0x82, "WireTemplateTest integer ignores unknown bits");
    expectOutOfRange([&]() { (void)bus.getBit(8); }, "WireTemplateTest getBit bounds");
    expectOutOfRange([&]() { bus.setBit(8, LogicValue::LOW); }, "WireTemplateTest setBit bounds");

    auto owner = std::make_shared<Component>("OWNER");
    Pin<8> pin("P", PinType::INPUT, owner);
    pin.setValueFromUInt64(0x3C);
    expect(pin.getWidth() == 8, "WireTemplateTest pin width");
    expect(pin.getValueAsUInt64() == 0x3C, "WireTemplateTest pin value");
    expect(pin.getBit(2) == LogicValue::HIGH, "WireTemplateTest pin bit 2");
    pin.setValueFromVector({LogicValue::HIGH, LogicValue::UNKNOWN, LogicValue::LOW, LogicValue::HIGH});
    expect(pin.getBit(0) == LogicValue::HIGH, "WireTemplateTest pin vector bit 0");
    expect(pin.getBit(1) == LogicValue::UNKNOWN, "WireTemplateTest pin vector bit 1");
    expect(pin.getBit(2) == LogicValue::LOW, "WireTemplateTest pin vector bit 2");
    expect(pin.getBit(3) == LogicValue::HIGH, "WireTemplateTest pin vector bit 3");
    expect(pin.getBit(4) == LogicValue::UNKNOWN, "WireTemplateTest pin vector short fill");
    expectOutOfRange([&]() { (void)pin.getBit(8); }, "WireTemplateTest pin getBit bounds");
    expectOutOfRange([&]() { pin.setBit(8, LogicValue::LOW); }, "WireTemplateTest pin setBit bounds");
}

std::string SimulatorAdvanceAndRecordTest::getTestName() const {
    return "SimulatorAdvanceAndRecordTest";
}

void SimulatorAdvanceAndRecordTest::verifyResults() {
    auto not_gate = Component::create<NOTGate>("NOT_ADVANCE_ROOT");
    ComponentBuilder local_builder(not_gate);
    auto io_root = std::dynamic_pointer_cast<IOComponent>(not_gate);
    assert(io_root);

    auto input_wire = local_builder.addNewWire("IN_WIRE", nullptr, {io_root->getInputPin("IN")});
    auto output_wire = local_builder.addNewWire("OUT_WIRE", io_root->getOutputPin("OUT"), {});

    Simulator local_sim;
    drive(local_sim, 0, input_wire, false);
    drive(local_sim, 10, input_wire, true);

    local_sim.advanceAndRecord(1);
    expectWireAt(local_sim, output_wire, 1, LogicValue::HIGH, "SimulatorAdvanceAndRecordTest first output");

    local_sim.advanceAndRecord(11);
    expectWireAt(local_sim, output_wire, 11, LogicValue::LOW, "SimulatorAdvanceAndRecordTest future output");
}

std::string SimulatorUnwiredOutputPinHistoryTest::getTestName() const {
    return "SimulatorUnwiredOutputPinHistoryTest";
}

void SimulatorUnwiredOutputPinHistoryTest::verifyResults() {
    auto splitter = Component::create<BitSplitter<8>>("SPLITTER_HISTORY_ROOT");
    ComponentBuilder local_builder(splitter);
    auto io_root = std::dynamic_pointer_cast<IOComponent>(splitter);
    assert(io_root);

    auto input_wire = local_builder.addNewWireDynamic(
        "IN_WIRE",
        8,
        nullptr,
        {io_root->getInputPinDynamic("IN")});

    // Keep one output connected and leave the others unwired. This reproduces
    // structural debug nodes such as overflow sign-bit splitters.
    auto out7 = io_root->getOutputPinDynamic("OUT_7");
    local_builder.addNewWireDynamic("OUT_7_WIRE", out7->getWidth(), out7, {});

    Simulator local_sim;
    local_sim.scheduleEvent(makeTestWireUpdate(0, input_wire, bits(0xff)));
    local_sim.scheduleEvent(makeTestWireUpdate(10, input_wire, bits(0x01)));
    local_sim.runAndRecord(20);

    local_sim.setCircuitStateAtTime(11);
    expectSplitterOutputs(io_root, 0x01, "SimulatorUnwiredOutputPinHistoryTest latest state");

    local_sim.setCircuitStateAtTime(1);
    expectSplitterOutputs(io_root, 0xff, "SimulatorUnwiredOutputPinHistoryTest restored state");
}

std::string SimulatorDrainUntilIdleTest::getTestName() const {
    return "SimulatorDrainUntilIdleTest";
}

void SimulatorDrainUntilIdleTest::verifyResults() {
    auto not_gate = Component::create<NOTGate>("NOT_DRAIN_ROOT");
    ComponentBuilder local_builder(not_gate);
    auto io_root = std::dynamic_pointer_cast<IOComponent>(not_gate);
    assert(io_root);

    auto input_wire = local_builder.addNewWire(
        "IN_WIRE", nullptr, {io_root->getInputPin("IN")});
    auto output_wire = local_builder.addNewWire(
        "OUT_WIRE", io_root->getOutputPin("OUT"), {});

    Simulator local_sim;
    drive(local_sim, 10, input_wire, false);

    auto deadline = local_sim.drainUntilIdle(9, 100);
    expect(deadline.status == DrainStatus::DeadlineReached,
           "SimulatorDrainUntilIdleTest deadline status");
    expect(deadline.processed_events == 0,
           "SimulatorDrainUntilIdleTest deadline event count");
    expect(local_sim.hasPendingEvents(),
           "SimulatorDrainUntilIdleTest pending after deadline");
    expect(local_sim.nextEventTime() == std::optional<size_t>{10},
           "SimulatorDrainUntilIdleTest next event time");

    auto limited = local_sim.drainUntilIdle(100, 1);
    expect(limited.status == DrainStatus::EventLimitReached,
           "SimulatorDrainUntilIdleTest event limit status");
    expect(limited.processed_events == 1,
           "SimulatorDrainUntilIdleTest event limit count");
    expect(local_sim.hasPendingEvents(),
           "SimulatorDrainUntilIdleTest pending after event limit");

    auto drained = local_sim.drainUntilIdle(100, 100);
    expect(drained.status == DrainStatus::Idle,
           "SimulatorDrainUntilIdleTest idle status");
    expect(!local_sim.hasPendingEvents(),
           "SimulatorDrainUntilIdleTest empty after drain");
    expect(output_wire->getSingleValue() == LogicValue::HIGH,
           "SimulatorDrainUntilIdleTest settled output");
}

NOTGateTest::NOTGateTest()
    : TruthTableComponentTest<NOTGate>("NOTGateTest", "NOT_GATE_ROOT", {
        {{{"IN", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"IN", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"IN", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"IN", logic(LogicValue::HIGH_Z)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }) {}

ANDGateTest::ANDGateTest()
    : TruthTableComponentTest<ANDGate>(
          "ANDGateTest",
          "AND_GATE_ROOT",
          binaryGateRows(BinaryGateOperation::And)) {}

ORGateTest::ORGateTest()
    : TruthTableComponentTest<ORGate>(
          "ORGateTest",
          "OR_GATE_ROOT",
          binaryGateRows(BinaryGateOperation::Or)) {}

XORGateTest::XORGateTest()
    : TruthTableComponentTest<XORGate>(
          "XORGateTest",
          "XOR_GATE_ROOT",
          binaryGateRows(BinaryGateOperation::Xor)) {}

NANDGateTest::NANDGateTest()
    : TruthTableComponentTest<NANDGate>(
          "NANDGateTest",
          "NAND_GATE_ROOT",
          binaryGateRows(BinaryGateOperation::Nand)) {}

NORGateTest::NORGateTest()
    : TruthTableComponentTest<NORGate>(
          "NORGateTest",
          "NOR_GATE_ROOT",
          binaryGateRows(BinaryGateOperation::Nor)) {}

namespace {
using circuit::test::ActionScenario;
using circuit::test::CheckpointKind;
using circuit::test::ComponentTestSpec;
using circuit::test::NamedValues;
using circuit::test::ScenarioAction;
using circuit::test::logicBit;

ScenarioAction sequentialAction(
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

ComponentTestSpec srLatchSpec() {
    ActionScenario scenario{
        "state-sequence",
        {},
        {
            sequentialAction(
                "reset",
                {{"S_BAR", logicBit(true)}, {"R_BAR", logicBit(false)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "hold-reset",
                {{"R_BAR", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "set",
                {{"S_BAR", logicBit(false)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(false)}}),
            sequentialAction(
                "hold-set",
                {{"S_BAR", logicBit(true)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(false)}}),
            sequentialAction(
                "invalid",
                {{"S_BAR", logicBit(false)}, {"R_BAR", logicBit(false)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "recover-reset",
                {{"S_BAR", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "high-z-set-input",
                {
                    {"S_BAR", logicBit(LogicValue::HIGH_Z)},
                    {"R_BAR", logicBit(true)},
                },
                {
                    {"Q", logicBit(LogicValue::UNKNOWN)},
                    {"Q_BAR", logicBit(LogicValue::UNKNOWN)},
                }),
            sequentialAction(
                "recover-from-high-z",
                {
                    {"S_BAR", logicBit(true)},
                    {"R_BAR", logicBit(false)},
                },
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
        },
        {100'000, 1'000'000},
    };
    return {
        "SRLatchTest",
        "sequential.sr-latch",
        "SR_LATCH_ROOT",
        {},
        {std::move(scenario)},
    };
}

ComponentTestSpec gatedDLatchSpec() {
    ActionScenario scenario{
        "state-sequence",
        {},
        {
            sequentialAction(
                "reset",
                {
                    {"D", logicBit(false)},
                    {"EN", logicBit(false)},
                    {"RST", logicBit(true)},
                },
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "release-reset",
                {{"RST", logicBit(false)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "disabled-hold",
                {{"D", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "transparent-high",
                {{"EN", logicBit(true)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(false)}}),
            sequentialAction(
                "transparent-low",
                {{"D", logicBit(false)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "disable",
                {{"EN", logicBit(false)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "hold-low",
                {{"D", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "re-enable",
                {{"EN", logicBit(true)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(false)}}),
            sequentialAction(
                "transparent-unknown",
                {{"D", logicBit(LogicValue::UNKNOWN)}},
                {
                    {"Q", logicBit(LogicValue::UNKNOWN)},
                    {"Q_BAR", logicBit(LogicValue::UNKNOWN)},
                }),
            sequentialAction(
                "reset-unknown",
                {{"RST", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "transparent-high-z",
                {
                    {"RST", logicBit(false)},
                    {"D", logicBit(LogicValue::HIGH_Z)},
                },
                {
                    {"Q", logicBit(LogicValue::UNKNOWN)},
                    {"Q_BAR", logicBit(LogicValue::UNKNOWN)},
                }),
            sequentialAction(
                "reset-high-z",
                {{"RST", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
        },
        {100'000, 2'000'000},
    };
    return {
        "GatedDLatchTest",
        "sequential.gated-d-latch",
        "GATED_D_LATCH_ROOT",
        {},
        {std::move(scenario)},
    };
}

ComponentTestSpec dFlipFlopSpec() {
    ActionScenario scenario{
        "edge-sequence",
        {},
        {
            sequentialAction(
                "reset",
                {
                    {"D", logicBit(false)},
                    {"CLK", logicBit(false)},
                    {"RST", logicBit(true)},
                },
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}},
                CheckpointKind::AfterEdge),
            sequentialAction(
                "release-reset",
                {{"RST", logicBit(false)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "prepare-one",
                {{"D", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "capture-one",
                {{"CLK", logicBit(true)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(false)}},
                CheckpointKind::AfterEdge),
            sequentialAction(
                "ignore-high-data-change",
                {{"D", logicBit(false)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(false)}}),
            sequentialAction(
                "falling-edge",
                {{"CLK", logicBit(false)}},
                {{"Q", logicBit(true)}, {"Q_BAR", logicBit(false)}},
                CheckpointKind::AfterEdge),
            sequentialAction(
                "capture-zero",
                {{"CLK", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}},
                CheckpointKind::AfterEdge),
            sequentialAction(
                "open-master",
                {{"CLK", logicBit(false)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}},
                CheckpointKind::AfterEdge),
            sequentialAction(
                "prepare-unknown",
                {{"D", logicBit(LogicValue::UNKNOWN)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "capture-unknown",
                {{"CLK", logicBit(true)}},
                {
                    {"Q", logicBit(LogicValue::UNKNOWN)},
                    {"Q_BAR", logicBit(LogicValue::UNKNOWN)},
                },
                CheckpointKind::AfterEdge),
            sequentialAction(
                "reset-unknown",
                {{"RST", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}},
                CheckpointKind::AfterEdge),
            sequentialAction(
                "prepare-high-z",
                {
                    {"RST", logicBit(false)},
                    {"CLK", logicBit(false)},
                    {"D", logicBit(LogicValue::HIGH_Z)},
                },
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}}),
            sequentialAction(
                "capture-high-z",
                {{"CLK", logicBit(true)}},
                {
                    {"Q", logicBit(LogicValue::UNKNOWN)},
                    {"Q_BAR", logicBit(LogicValue::UNKNOWN)},
                },
                CheckpointKind::AfterEdge),
            sequentialAction(
                "reset-high-z",
                {{"RST", logicBit(true)}},
                {{"Q", logicBit(false)}, {"Q_BAR", logicBit(true)}},
                CheckpointKind::AfterEdge),
        },
        {100'000, 2'000'000},
    };
    return {
        "DFlipFlopTest",
        "sequential.d-flip-flop",
        "DFF_ROOT",
        {},
        {std::move(scenario)},
    };
}
} // namespace

SRLatchTest::SRLatchTest()
    : ComponentScenarioTest(srLatchSpec()) {}

GatedDLatchTest::GatedDLatchTest()
    : ComponentScenarioTest(gatedDLatchSpec()) {}

DFlipFlopTest::DFlipFlopTest()
    : ComponentScenarioTest(dFlipFlopSpec()) {}

std::string ClockGeneratorTest::getTestName() const {
    return "ClockGeneratorTest";
}

void ClockGeneratorTest::setupCircuit() {
    auto profile = circuit::withExactFidelity(
        circuit::canonicalDefaultProfile(),
        "CLK_ROOT",
        circuit::Fidelity::Structural,
        "clock-generator-test");
    auto build = circuit::builtinComponentCatalog().createRoot(
        {
            "timing.clock",
            "CLK_ROOT",
            {{"half_period", "5"}},
            {},
            {},
            {},
        },
        std::move(profile));
    root = std::move(build.root);
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void ClockGeneratorTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "ClockGeneratorTest requires IOComponent root");

    builder->addNewWire("CLK_OUT", io_root->getOutputPin("CLK_OUT"), {});
    auto clock = std::dynamic_pointer_cast<ClockGenerator>(root);
    assert(clock && "ClockGeneratorTest root is not ClockGenerator");
    clock->startClock(*sim, 0);
}

void ClockGeneratorTest::verifyResults() {
    auto clock_wire = builder->getWire("CLK_OUT");
    expectWireAt(*sim, clock_wire, 0, LogicValue::HIGH, "CLK_OUT");
    expectWireAt(*sim, clock_wire, 5, LogicValue::LOW, "CLK_OUT");
    expectWireAt(*sim, clock_wire, 10, LogicValue::HIGH, "CLK_OUT");
    expectWireAt(*sim, clock_wire, 15, LogicValue::LOW, "CLK_OUT");
    expectWireAt(*sim, clock_wire, 20, LogicValue::HIGH, "CLK_OUT");
}

size_t ClockGeneratorTest::getRunDuration() const {
    return 20;
}

std::vector<SimulationTest::SimulationCheckpoint>
ClockGeneratorTest::getCheckpoints() const {
    return {
        {0, "transition-0", "CLK_OUT=1", 0},
        {5, "transition-1", "CLK_OUT=0", 1},
        {10, "transition-2", "CLK_OUT=1", 2},
        {15, "transition-3", "CLK_OUT=0", 3},
        {20, "transition-4", "CLK_OUT=1", 4},
    };
}
