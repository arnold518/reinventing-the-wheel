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
    return "FullCircuitTest";
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
    auto clk_gen = builder->addNewComponent<ClockGenerator>("CLK_GEN", 50);

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
    
    auto wire_rst = builder->addNewWire("RST_IN", nullptr, {
        builder->getInputPin<DFlipFlop>("DFF1", "RST")
    });
    wire_rst->setValue(LogicValue::LOW);

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
    auto wire_rst = builder->getWire("RST_IN");
    auto clk_gen = builder->getComponent<ClockGenerator>("CLK_GEN");
    
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_rst, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(30, wire_rst, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_a, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_b, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(20, wire_b, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(160, wire_a, LogicValue::LOW));
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
    expectWireAt(*sim, wire_b, 20, LogicValue::HIGH, "WireB");
    expectWireAt(*sim, and1_out, 30, LogicValue::HIGH, "WireAND1_OUT");

    expectWireAt(*sim, clock, 100, LogicValue::HIGH, "WireCLK");
    expectWireAt(*sim, clock, 150, LogicValue::LOW, "WireCLK");
    expectWireAt(*sim, clock, 200, LogicValue::HIGH, "WireCLK");

    // The structural DFF needs internal gate settle time after each rising edge.
    expectWireAt(*sim, dff_q_wire, 90, LogicValue::LOW, "WireDFF1_Q after reset");
    expectWireAt(*sim, dff_q_wire, 140, LogicValue::HIGH, "WireDFF1_Q captures high");
    expectWireAt(*sim, dff_q_bar_wire, 140, LogicValue::LOW, "DFF1_Q_BAR_Wire captures high");
    expectWireAt(*sim, z_wire, 145, LogicValue::HIGH, "WireAND2_OUT after first capture");

    expectWireAt(*sim, wire_a, 160, LogicValue::LOW, "WireA");
    expectWireAt(*sim, and1_out, 170, LogicValue::LOW, "WireAND1_OUT");
    expectWireAt(*sim, dff_q_wire, 190, LogicValue::HIGH, "WireDFF1_Q holds before second rising edge");
    expectWireAt(*sim, dff_q_wire, 240, LogicValue::LOW, "WireDFF1_Q captures low");
    expectWireAt(*sim, dff_q_bar_wire, 240, LogicValue::HIGH, "DFF1_Q_BAR_Wire captures low");
    expectWireAt(*sim, z_wire, 245, LogicValue::LOW, "WireAND2_OUT after second capture");
}

size_t FullCircuitTest::getRunDuration() const {
    return 260;
}
