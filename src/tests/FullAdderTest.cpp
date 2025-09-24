#include "tests/FullAdderTest.hpp"
#include "components/ComponentBuilder.tpp" // Must be included for template definitions
#include "components/ComponentBuilder.hpp"
#include "modules/composite/FullAdder.hpp" // The component we are testing
#include "basic/Wire.hpp"
#include "basic/Pin.hpp"
#include "simulator/Event.hpp"
#include <cassert>

std::string FullAdderTest::getTestName() const {
    return "FullAdderTest";
}

// Override setupCircuit to make the FullAdder itself the root.
void FullAdderTest::setupCircuit() {
    // Create a FullAdder instance as the top-level component for this test.
    root = Component::create<FullAdder>("FA_ROOT");
    
    // Initialize the builder with this FullAdder as the root context.
    builder = std::make_unique<ComponentBuilder>(root);

    buildCircuit();
    setInitialState();
}

void FullAdderTest::buildCircuit() {
    // The FullAdder root builds its own internal components via its buildInternals method.
    // This method is intentionally left empty.
}

void FullAdderTest::setInitialState() {
    // Create source-less wires to act as the test inputs and connect them.
    auto wire_a = builder->addNewWire("INPUT_A", nullptr, { builder->getInputPin("A") });
    auto wire_b = builder->addNewWire("INPUT_B", nullptr, { builder->getInputPin("B") });
    auto wire_cin = builder->addNewWire("INPUT_CIN", nullptr, { builder->getInputPin("Carry_in") });

    // Schedule events to test the full truth table (8 cases)
    // T=0:  A=0, B=0, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_a, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_b, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_cin, LogicValue::LOW));
    
    // T=10: A=0, B=0, Cin=1
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(10, wire_cin, LogicValue::HIGH));
    
    // T=20: A=0, B=1, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_b, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_cin, LogicValue::LOW));
    
    // T=30: A=0, B=1, Cin=1
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(30, wire_cin, LogicValue::HIGH));
    
    // T=40: A=1, B=0, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(40, wire_a, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(40, wire_b, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(40, wire_cin, LogicValue::LOW));

    // T=50: A=1, B=0, Cin=1
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(50, wire_cin, LogicValue::HIGH));

    // T=60: A=1, B=1, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(60, wire_b, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(60, wire_cin, LogicValue::LOW));

    // T=70: A=1, B=1, Cin=1 (Final state for verification)
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(70, wire_cin, LogicValue::HIGH));
}

void FullAdderTest::verifyResults() {
    // At the end of the simulation, the inputs will be A=1, B=1, Cin=1.
    // The expected result is Sum = 1, Carry_out = 1.

    auto pin_sum = builder->getOutputPin("Sum");
    auto pin_carry = builder->getOutputPin("Carry_out");
    
    std::cout << "Verification: Checking final state of FullAdder..." << std::endl;
    assert(pin_sum->getValue() == LogicValue::HIGH && "Sum output was expected to be HIGH.");
    assert(pin_carry->getValue() == LogicValue::HIGH && "Carry_out output was expected to be HIGH.");
}

size_t FullAdderTest::getRunDuration() const {
    // Allow time for the final (t=70) event to propagate through the circuit layers.
    return 80;
}