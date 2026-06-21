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

NOTGateTest::NOTGateTest()
    : ComponentRowsTest<NOTGate>("NOTGateTest", "NOT_GATE_ROOT", {
        {{{"IN", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"IN", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"IN", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"IN", logic(LogicValue::HIGH_Z)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }) {}

ANDGateTest::ANDGateTest()
    : ComponentRowsTest<ANDGate>("ANDGateTest", "AND_GATE_ROOT", {
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(false)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(true)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(true)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::HIGH_Z)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }) {}

ORGateTest::ORGateTest()
    : ComponentRowsTest<ORGate>("ORGateTest", "OR_GATE_ROOT", {
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(true)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::HIGH_Z)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }) {}

XORGateTest::XORGateTest()
    : ComponentRowsTest<XORGate>("XORGateTest", "XOR_GATE_ROOT", {
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(true)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::HIGH_Z)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }) {}

NANDGateTest::NANDGateTest()
    : ComponentRowsTest<NANDGate>("NANDGateTest", "NAND_GATE_ROOT", {
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(true)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(true)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(true)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }) {}

NORGateTest::NORGateTest()
    : ComponentRowsTest<NORGate>("NORGateTest", "NOR_GATE_ROOT", {
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(false)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::UNKNOWN)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }) {}

std::string SRLatchTest::getTestName() const {
    return "SRLatchTest";
}

void SRLatchTest::setupCircuit() {
    root = Component::create<SRLatch>("SR_LATCH_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void SRLatchTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "SRLatchTest requires IOComponent root");

    auto s_bar_wire = builder->addNewWire("S_BAR_IN", nullptr, {io_root->getInputPin("S_BAR")});
    auto r_bar_wire = builder->addNewWire("R_BAR_IN", nullptr, {io_root->getInputPin("R_BAR")});
    builder->addNewWire("Q_OUT", io_root->getOutputPin("Q"), {});
    builder->addNewWire("Q_BAR_OUT", io_root->getOutputPin("Q_BAR"), {});

    drive(*sim, 0, s_bar_wire, true);
    drive(*sim, 0, r_bar_wire, false);
    drive(*sim, 20, r_bar_wire, true);
    drive(*sim, 40, s_bar_wire, false);
    drive(*sim, 60, s_bar_wire, true);
    drive(*sim, 80, s_bar_wire, false);
    drive(*sim, 80, r_bar_wire, false);
    drive(*sim, 100, s_bar_wire, true);
    drive(*sim, 100, r_bar_wire, false);
}

void SRLatchTest::verifyResults() {
    auto q_wire = builder->getWire("Q_OUT");
    auto q_bar_wire = builder->getWire("Q_BAR_OUT");

    expectWireAt(*sim, q_wire, 10, LogicValue::LOW, "Q_OUT reset");
    expectWireAt(*sim, q_bar_wire, 10, LogicValue::HIGH, "Q_BAR_OUT reset");
    expectWireAt(*sim, q_wire, 30, LogicValue::LOW, "Q_OUT hold reset");
    expectWireAt(*sim, q_bar_wire, 30, LogicValue::HIGH, "Q_BAR_OUT hold reset");
    expectWireAt(*sim, q_wire, 50, LogicValue::HIGH, "Q_OUT set");
    expectWireAt(*sim, q_bar_wire, 50, LogicValue::LOW, "Q_BAR_OUT set");
    expectWireAt(*sim, q_wire, 70, LogicValue::HIGH, "Q_OUT hold set");
    expectWireAt(*sim, q_bar_wire, 70, LogicValue::LOW, "Q_BAR_OUT hold set");
    expectWireAt(*sim, q_wire, 90, LogicValue::HIGH, "Q_OUT invalid active-low inputs");
    expectWireAt(*sim, q_bar_wire, 90, LogicValue::HIGH, "Q_BAR_OUT invalid active-low inputs");
    expectWireAt(*sim, q_wire, 110, LogicValue::LOW, "Q_OUT reset after invalid");
    expectWireAt(*sim, q_bar_wire, 110, LogicValue::HIGH, "Q_BAR_OUT reset after invalid");
}

size_t SRLatchTest::getRunDuration() const {
    return 120;
}

std::vector<SimulationTest::SimulationCheckpoint> SRLatchTest::getCheckpoints() const {
    return {
        {10, "Reset", "S_BAR=1, R_BAR=0 -> Q=0", 0},
        {30, "Hold reset", "S_BAR=1, R_BAR=1 -> Q holds 0", 1},
        {50, "Set", "S_BAR=0, R_BAR=1 -> Q=1", 2},
        {70, "Hold set", "S_BAR=1, R_BAR=1 -> Q holds 1", 3},
        {90, "Invalid", "S_BAR=0, R_BAR=0 -> both NAND outputs high", 4},
        {110, "Reset again", "R_BAR=0 recovers Q=0", 5},
    };
}

std::string GatedDLatchTest::getTestName() const {
    return "GatedDLatchTest";
}

void GatedDLatchTest::setupCircuit() {
    root = Component::create<GatedDLatch>("GATED_D_LATCH_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void GatedDLatchTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "GatedDLatchTest requires IOComponent root");

    auto d_wire = builder->addNewWire("D_IN", nullptr, {io_root->getInputPin("D")});
    auto en_wire = builder->addNewWire("EN_IN", nullptr, {io_root->getInputPin("EN")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire("Q_OUT", io_root->getOutputPin("Q"), {});
    builder->addNewWire("Q_BAR_OUT", io_root->getOutputPin("Q_BAR"), {});

    drive(*sim, 0, d_wire, false);
    drive(*sim, 0, en_wire, false);
    drive(*sim, 0, rst_wire, true);
    drive(*sim, 20, rst_wire, false);
    drive(*sim, 30, d_wire, true);
    drive(*sim, 50, en_wire, true);
    drive(*sim, 80, d_wire, false);
    drive(*sim, 110, en_wire, false);
    drive(*sim, 120, d_wire, true);
    drive(*sim, 150, en_wire, true);
    drive(*sim, 180, d_wire, LogicValue::UNKNOWN);
    drive(*sim, 220, rst_wire, true);
}

void GatedDLatchTest::verifyResults() {
    auto q_wire = builder->getWire("Q_OUT");
    auto q_bar_wire = builder->getWire("Q_BAR_OUT");

    expectWireAt(*sim, q_wire, 10, LogicValue::LOW, "Q_OUT reset");
    expectWireAt(*sim, q_bar_wire, 10, LogicValue::HIGH, "Q_BAR_OUT reset");
    expectWireAt(*sim, q_wire, 45, LogicValue::LOW, "Q_OUT disabled hold");
    expectWireAt(*sim, q_wire, 70, LogicValue::HIGH, "Q_OUT enabled set");
    expectWireAt(*sim, q_bar_wire, 70, LogicValue::LOW, "Q_BAR_OUT enabled set");
    expectWireAt(*sim, q_wire, 100, LogicValue::LOW, "Q_OUT transparent low");
    expectWireAt(*sim, q_bar_wire, 100, LogicValue::HIGH, "Q_BAR_OUT transparent low");
    expectWireAt(*sim, q_wire, 140, LogicValue::LOW, "Q_OUT disabled hold low");
    expectWireAt(*sim, q_wire, 170, LogicValue::HIGH, "Q_OUT re-enabled high");
    expectWireAt(*sim, q_wire, 205, LogicValue::UNKNOWN, "Q_OUT transparent unknown");
    expectWireAt(*sim, q_bar_wire, 205, LogicValue::UNKNOWN, "Q_BAR_OUT transparent unknown");
    expectWireAt(*sim, q_wire, 240, LogicValue::LOW, "Q_OUT reset clears unknown");
    expectWireAt(*sim, q_bar_wire, 240, LogicValue::HIGH, "Q_BAR_OUT reset clears unknown");
}

size_t GatedDLatchTest::getRunDuration() const {
    return 250;
}

std::vector<SimulationTest::SimulationCheckpoint> GatedDLatchTest::getCheckpoints() const {
    return {
        {10, "Reset", "RST=1 clears Q", 0},
        {45, "Disabled hold", "EN=0, D=1 -> Q holds 0", 1},
        {70, "Transparent high", "EN=1, D=1 -> Q=1", 2},
        {100, "Transparent low", "EN=1, D=0 -> Q=0", 3},
        {140, "Hold low", "EN=0, D=1 -> Q holds 0", 4},
        {170, "Re-enabled high", "EN=1, D=1 -> Q=1", 5},
        {205, "Transparent unknown", "EN=1, D=X -> Q=X", 6},
        {240, "Reset unknown", "RST=1 clears Q from X to 0", 7},
    };
}

std::string DFlipFlopTest::getTestName() const {
    return "DFlipFlopTest";
}

void DFlipFlopTest::setupCircuit() {
    root = Component::create<DFlipFlop>("DFF_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void DFlipFlopTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "DFlipFlopTest requires IOComponent root");

    auto d_wire = builder->addNewWire("D_IN", nullptr, {io_root->getInputPin("D")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire("Q_OUT", io_root->getOutputPin("Q"), {});
    builder->addNewWire("Q_BAR_OUT", io_root->getOutputPin("Q_BAR"), {});

    drive(*sim, 0, rst_wire, true);
    drive(*sim, 0, d_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 30, rst_wire, false);
    drive(*sim, 40, d_wire, true);
    drive(*sim, 70, clk_wire, true);
    drive(*sim, 100, d_wire, false);
    drive(*sim, 130, clk_wire, false);
    drive(*sim, 170, clk_wire, true);
    drive(*sim, 210, d_wire, LogicValue::UNKNOWN);
    drive(*sim, 240, clk_wire, false);
    drive(*sim, 270, clk_wire, true);
    drive(*sim, 320, rst_wire, true);
}

void DFlipFlopTest::verifyResults() {
    auto q_wire = builder->getWire("Q_OUT");
    auto q_bar_wire = builder->getWire("Q_BAR_OUT");

    expectWireAt(*sim, q_wire, 20, LogicValue::LOW, "Q_OUT reset");
    expectWireAt(*sim, q_bar_wire, 20, LogicValue::HIGH, "Q_BAR_OUT reset");
    expectWireAt(*sim, q_wire, 60, LogicValue::LOW, "Q_OUT low before first rising edge");
    expectWireAt(*sim, q_wire, 95, LogicValue::HIGH, "Q_OUT captures D=1 on rising edge");
    expectWireAt(*sim, q_bar_wire, 95, LogicValue::LOW, "Q_BAR_OUT captures D=1 on rising edge");
    expectWireAt(*sim, q_wire, 120, LogicValue::HIGH, "Q_OUT ignores D change while CLK high");
    expectWireAt(*sim, q_wire, 155, LogicValue::HIGH, "Q_OUT ignores falling edge");
    expectWireAt(*sim, q_wire, 200, LogicValue::LOW, "Q_OUT captures D=0 on next rising edge");
    expectWireAt(*sim, q_bar_wire, 200, LogicValue::HIGH, "Q_BAR_OUT captures D=0 on next rising edge");
    expectWireAt(*sim, q_wire, 260, LogicValue::LOW, "Q_OUT holds while master samples unknown");
    expectWireAt(*sim, q_wire, 300, LogicValue::UNKNOWN, "Q_OUT captures unknown D on rising edge");
    expectWireAt(*sim, q_bar_wire, 300, LogicValue::UNKNOWN, "Q_BAR_OUT captures unknown D on rising edge");
    expectWireAt(*sim, q_wire, 345, LogicValue::LOW, "Q_OUT reset clears unknown");
    expectWireAt(*sim, q_bar_wire, 345, LogicValue::HIGH, "Q_BAR_OUT reset clears unknown");
}

size_t DFlipFlopTest::getRunDuration() const {
    return 360;
}

std::vector<SimulationTest::SimulationCheckpoint> DFlipFlopTest::getCheckpoints() const {
    return {
        {20, "Reset", "RST=1 clears Q", 0},
        {60, "Pre-edge hold", "D=1 while CLK=0 updates master only; Q remains 0", 1},
        {95, "Rising capture one", "CLK rises and slave publishes D=1", 2},
        {120, "No high-level capture", "D changes while CLK is already high; Q remains 1", 3},
        {155, "No falling capture", "CLK falls; Q remains 1", 4},
        {200, "Rising capture zero", "Next rising edge publishes D=0", 5},
        {300, "Capture unknown", "Unknown D sampled by master and published on next rising edge", 6},
        {345, "Reset unknown", "RST=1 clears Q from X to 0", 7},
    };
}

std::string ClockGeneratorTest::getTestName() const {
    return "ClockGeneratorTest";
}

void ClockGeneratorTest::setupCircuit() {
    root = Component::create<ClockGenerator>("CLK_ROOT", 5);
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
