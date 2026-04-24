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
    auto clk_gen = builder->getComponent<ClockGenerator>("CLK_GEN");
    
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_a, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent<>>(0, wire_b, LogicValue::LOW));
    clk_gen->startClock(*sim, 0);
}

void FullCircuitTest::verifyResults() {
    // Verification logic remains the same. We check an internal wire's state.
    auto dff_q_wire = builder->getWire("WireDFF1_Q");
    
    std::cout << "Verification: Checking final value of DFF1:Q..." << std::endl;
    assert(dff_q_wire->getSingleValue() == LogicValue::LOW && "DFF output 'Q' was expected to be LOW but was HIGH.");
}

size_t FullCircuitTest::getRunDuration() const {
    return 100;
}
