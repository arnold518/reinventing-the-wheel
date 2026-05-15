#include "tests/FullCircuitTest.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp" 
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "simulator/Event.hpp"
#include "basic/Wire.hpp"
#include <cassert>
#include <iostream>

namespace {
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

std::string FullCircuitTest::getTestName() const {
    return "Full_Circuit_Test";
}

// Override the base setupCircuit to create a root of type IOComponent
void FullCircuitTest::setupCircuit() {
    // Create the root using the new on-the-fly pin definition feature
    root = Component::create<IOComponent>(getTestName(), 
        [](IOComponent* self){
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("Z", PinType::OUTPUT);
        }
    );
    
    // The builder is now initialized with our new IOComponent root
    builder = std::make_unique<ComponentBuilder>(root);

    buildCircuit();
    setInitialState();
}

void FullCircuitTest::buildCircuit() {
    // Create the internal components. The builder correctly adds these
    // as children of our IOComponent root.
    builder->addNewComponent<ANDGate>("AND1");
    builder->addNewComponent<DFlipFlop>("DFF1");
    builder->addNewComponent<ANDGate>("AND2");
    auto clk_gen = builder->addNewComponent<ClockGenerator>("CLK_GEN", 5);

    // --- Connect internal components to the root's pins ---
    // The source of the wire is now the root's "A" pin.
    auto wire_a = builder->addNewWire("WireA", builder->getInputPin("A"), {
        builder->getInputPin<ANDGate>("AND1", "A")
    });
    // The source of the wire is now the root's "B" pin.
    auto wire_b = builder->addNewWire("WireB", builder->getInputPin("B"), {
        builder->getInputPin<ANDGate>("AND1", "B"),
        builder->getInputPin<ANDGate>("AND2", "B")
    });
    
    auto wire_gnd = builder->addNewWire("GND", nullptr, {
        builder->getInputPin<DFlipFlop>("DFF1", "RST")
    });
    wire_gnd->setValue(LogicValue::LOW);

    builder->addNewWire("WireAND1_OUT", builder->getOutputPin<ANDGate>("AND1", "OUT"), { builder->getInputPin<DFlipFlop>("DFF1", "D") });
    builder->addNewWire("WireCLK", builder->getOutputPin<ClockGenerator>("CLK_GEN", "CLK_OUT"), { builder->getInputPin<DFlipFlop>("DFF1", "CLK") });
    builder->addNewWire("WireDFF1_Q", builder->getOutputPin<DFlipFlop>("DFF1", "Q"), { builder->getInputPin<ANDGate>("AND2", "A") });
    
    // The final output wire now connects to the root's "Z" pin.
    builder->addNewWire("WireAND2_OUT", builder->getOutputPin<ANDGate>("AND2", "OUT"), { builder->getOutputPin("Z") });
    
    builder->addNewWire("DFF1_Q_BAR_Wire", builder->getOutputPin<DFlipFlop>("DFF1", "Q_BAR"), {});
}

void FullCircuitTest::setInitialState() {
    // Get the wires connected to the root's input pins to set their initial state.
    auto wire_a = builder->getWire("WireA");
    auto wire_b = builder->getWire("WireB");
    auto wire_gnd = builder->getWire("GND");
    auto clk_gen = builder->getComponent<ClockGenerator>("CLK_GEN");
    
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_gnd, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_a, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_b, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(2, wire_b, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(16, wire_a, LogicValue::LOW));
    clk_gen->startClock(*sim, 0);
}

void FullCircuitTest::verifyResults() {
    auto wire_a = builder->getWire("WireA");
    auto wire_b = builder->getWire("WireB");
    auto and1_out = builder->getWire("WireAND1_OUT");
    auto clock = builder->getWire("WireCLK");
    auto dff_q_wire = builder->getWire("WireDFF1_Q");
    auto dff_q_bar_wire = builder->getWire("DFF1_Q_BAR_Wire");
    auto z_wire = builder->getWire("WireAND2_OUT");

    std::cout << "Verification: Checking FullCircuitTest signal history..." << std::endl;

    // Primary inputs and first combinational stage settle immediately after t=0.
    expectWireAt(*sim, wire_a, 0, LogicValue::HIGH, "WireA");
    expectWireAt(*sim, wire_b, 0, LogicValue::LOW, "WireB");
    expectWireAt(*sim, and1_out, 0, LogicValue::UNKNOWN, "WireAND1_OUT");
    expectWireAt(*sim, and1_out, 1, LogicValue::LOW, "WireAND1_OUT");
    expectWireAt(*sim, z_wire, 1, LogicValue::LOW, "WireAND2_OUT");
    expectWireAt(*sim, wire_b, 2, LogicValue::HIGH, "WireB");
    expectWireAt(*sim, and1_out, 3, LogicValue::HIGH, "WireAND1_OUT");
    expectWireAt(*sim, z_wire, 3, LogicValue::UNKNOWN, "WireAND2_OUT");

    // ClockGenerator toggles every 5 time units after it starts at t=0.
    expectWireAt(*sim, clock, 0, LogicValue::HIGH, "WireCLK");
    expectWireAt(*sim, clock, 5, LogicValue::LOW, "WireCLK");
    expectWireAt(*sim, clock, 10, LogicValue::HIGH, "WireCLK");
    expectWireAt(*sim, clock, 15, LogicValue::LOW, "WireCLK");
    expectWireAt(*sim, clock, 20, LogicValue::HIGH, "WireCLK");

    // DFF captures the combinational D value only on rising edges and publishes Q after its delay of 3.
    expectWireAt(*sim, dff_q_wire, 12, LogicValue::UNKNOWN, "WireDFF1_Q");
    expectWireAt(*sim, dff_q_wire, 13, LogicValue::HIGH, "WireDFF1_Q");
    expectWireAt(*sim, dff_q_bar_wire, 13, LogicValue::LOW, "DFF1_Q_BAR_Wire");
    expectWireAt(*sim, z_wire, 14, LogicValue::HIGH, "WireAND2_OUT");

    expectWireAt(*sim, wire_a, 16, LogicValue::LOW, "WireA");
    expectWireAt(*sim, and1_out, 17, LogicValue::LOW, "WireAND1_OUT");
    expectWireAt(*sim, dff_q_wire, 22, LogicValue::HIGH, "WireDFF1_Q");
    expectWireAt(*sim, dff_q_wire, 23, LogicValue::LOW, "WireDFF1_Q");
    expectWireAt(*sim, dff_q_bar_wire, 23, LogicValue::HIGH, "DFF1_Q_BAR_Wire");
    expectWireAt(*sim, z_wire, 24, LogicValue::LOW, "WireAND2_OUT");
    expectWireAt(*sim, dff_q_wire, 100, LogicValue::LOW, "WireDFF1_Q");
}

size_t FullCircuitTest::getRunDuration() const {
    return 100;
}
