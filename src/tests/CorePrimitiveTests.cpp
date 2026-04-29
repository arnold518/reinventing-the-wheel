#include "tests/CorePrimitiveTests.hpp"

#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/Gate.hpp"
#include "simulator/Event.hpp"
#include <cassert>
#include <iostream>

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

    auto owner = std::make_shared<Component>("OWNER");
    Pin<8> pin("P", PinType::INPUT, owner);
    pin.setValueFromUInt64(0x3C);
    expect(pin.getWidth() == 8, "WireTemplateTest pin width");
    expect(pin.getValueAsUInt64() == 0x3C, "WireTemplateTest pin value");
    expect(pin.getBit(2) == LogicValue::HIGH, "WireTemplateTest pin bit 2");
}

std::string GateTest::getTestName() const {
    return "GateTest";
}

void GateTest::verifyResults() {
    expect(runRows<NOTGate>({
        {{{"IN", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"IN", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"IN", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"IN", logic(LogicValue::HIGH_Z)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }), "GateTest NOTGate");
    expect(runRows<ANDGate>({
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(true)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }), "GateTest ANDGate");
    expect(runRows<ORGate>({
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }), "GateTest ORGate");
    expect(runRows<XORGate>({
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
        {{{"A", logic(LogicValue::HIGH_Z)}, {"B", bit(false)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }), "GateTest XORGate");
    expect(runRows<NANDGate>({
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }), "GateTest NANDGate");
    expect(runRows<NORGate>({
        {{{"A", bit(false)}, {"B", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"A", bit(false)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", bit(true)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(true)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", logic(LogicValue::UNKNOWN)}}, {{"OUT", logic(LogicValue::UNKNOWN)}}},
    }), "GateTest NORGate");
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

    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, rst_wire, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, d_wire, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, clk_wire, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(5, clk_wire, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(10, clk_wire, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(10, d_wire, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(12, rst_wire, LogicValue::HIGH));
}

void DFlipFlopTest::verifyResults() {
    auto q_wire = builder->getWire("Q_OUT");
    auto q_bar_wire = builder->getWire("Q_BAR_OUT");

    expectWireAt(*sim, q_wire, 7, LogicValue::UNKNOWN, "Q_OUT");
    expectWireAt(*sim, q_wire, 8, LogicValue::HIGH, "Q_OUT");
    expectWireAt(*sim, q_bar_wire, 8, LogicValue::LOW, "Q_BAR_OUT");
    expectWireAt(*sim, q_wire, 11, LogicValue::HIGH, "Q_OUT");
    expectWireAt(*sim, q_wire, 15, LogicValue::LOW, "Q_OUT");
    expectWireAt(*sim, q_bar_wire, 15, LogicValue::HIGH, "Q_BAR_OUT");
}

size_t DFlipFlopTest::getRunDuration() const {
    return 30;
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
