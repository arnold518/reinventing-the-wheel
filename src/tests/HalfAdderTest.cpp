#include "tests/HalfAdderTest.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/IOComponent.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include <cassert>

std::string HalfAdderTest::getTestName() const {
    return "HalfAdderTest";
}

/*

// Override the base setupCircuit to create a root of type IOComponent
void HalfAdderTest::setupCircuit() {
    // Create the root using the on-the-fly pin definition feature.
    // This defines the external interface for our entire test.
    root = Component::create<IOComponent>(getTestName(),
        [](IOComponent* self){
            self->addPin("TestA", PinType::INPUT);
            self->addPin("TestB", PinType::INPUT);
            self->addPin("TestSum", PinType::OUTPUT);
            self->addPin("TestCarry", PinType::OUTPUT);
        }
    );
    
    // The builder is now initialized with our new IOComponent root
    builder = std::make_unique<ComponentBuilder>(root);

    // Proceed with the standard setup flow
    buildCircuit();
    setInitialState();
}

void HalfAdderTest::buildCircuit() {
    // 1. Instantiate the component under test.
    // The builder correctly adds this as a child of our IOComponent root.
    builder->addNewComponent<HalfAdder>("HA1");

    // 2. Wire the root's interface to the HalfAdder's interface.
    // This demonstrates the hierarchical wiring logic.
    builder->addNewWire("WireA_internal",
        builder->getInputPin("TestA"), // Source: Parent's INPUT
        { builder->getInputPin<HalfAdder>("HA1", "A") } // Sink: Child's INPUT
    );
    builder->addNewWire("WireB_internal",
        builder->getInputPin("TestB"), // Source: Parent's INPUT
        { builder->getInputPin<HalfAdder>("HA1", "B") } // Sink: Child's INPUT
    );
    builder->addNewWire("WireSum_internal",
        builder->getOutputPin<HalfAdder>("HA1", "Sum"), // Source: Child's OUTPUT
        { builder->getOutputPin("TestSum") } // Sink: Parent's OUTPUT
    );
    builder->addNewWire("WireCarry_internal",
        builder->getOutputPin<HalfAdder>("HA1", "Carry"), // Source: Child's OUTPUT
        { builder->getOutputPin("TestCarry") } // Sink: Parent's OUTPUT
    );
}

void HalfAdderTest::setInitialState() {
    // Get handles to the wires connected to the root's input pins.
    auto wire_a = builder->getWire("WireA_internal");
    auto wire_b = builder->getWire("WireB_internal");

    // Schedule events to test all 4 cases of a half adder's truth table.
    // Initial state (t=0): A=0, B=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_a, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_b, LogicValue::LOW));
    
    // t=10: A=0, B=1
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(10, wire_b, LogicValue::HIGH));
    
    // t=20: A=1, B=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_a, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_b, LogicValue::LOW));
    
    // t=30: A=1, B=1 (This will be the final state for verification)
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(30, wire_b, LogicValue::HIGH));
}

void HalfAdderTest::verifyResults() {
    // At the end of the simulation, the inputs will be A=1, B=1.
    // We expect Sum=0 and Carry=1.
    auto wire_sum = builder->getWire("WireSum_internal");
    auto wire_carry = builder->getWire("WireCarry_internal");
    
    std::cout << "Verification: Checking final state of HalfAdder..." << std::endl;
    assert(wire_sum->getValue() == LogicValue::LOW && "Sum output was expected to be LOW.");
    assert(wire_carry->getValue() == LogicValue::HIGH && "Carry output was expected to be HIGH.");
}

*/

// Override setupCircuit to make the HalfAdder itself the root.
void HalfAdderTest::setupCircuit() {
    // 1. Create a HalfAdder instance as the top-level component for this test.
    root = Component::create<HalfAdder>("HA_ROOT");
    
    // 2. Initialize the builder with this HalfAdder as the root context.
    builder = std::make_unique<ComponentBuilder>(root);

    // 3. Proceed with the standard setup flow.
    buildCircuit();
    setInitialState();
}

void HalfAdderTest::buildCircuit() {
    // The HalfAdder root builds its own internal components (XOR, AND) via its
    // buildInternals method, which is called automatically by Component::create.
    // Therefore, this method is intentionally left empty.
}

void HalfAdderTest::setInitialState() {
    // Create source-less wires to act as the test inputs.
    // The builder connects them directly to the root HalfAdder's input pins.
    auto wire_a = builder->addNewWire("INPUT_A", nullptr, { builder->getInputPin("A") });
    auto wire_b = builder->addNewWire("INPUT_B", nullptr, { builder->getInputPin("B") });

    // Schedule events to test all 4 cases of the half adder's truth table.
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_a, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_b, LogicValue::LOW));
    
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(10, wire_b, LogicValue::HIGH));
    
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_a, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_b, LogicValue::LOW));
    
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(30, wire_b, LogicValue::HIGH));
}

void HalfAdderTest::verifyResults() {
    // At the end of the simulation, the inputs will be A=1, B=1.
    // We expect Sum=0 and Carry=1.

    // To check the results, we get the HalfAdder's output pins...
    auto pin_sum = builder->getOutputPin("Sum");
    auto pin_carry = builder->getOutputPin("Carry");

    // ...and then get the external wires connected to them.
    auto wire_sum = pin_sum->getExternalWire();
    auto wire_carry = pin_carry->getExternalWire();

    // The builder does not create these output wires, so we just check the pin values.
    // NOTE: A robust test might create external "probe" wires. Checking the pin is sufficient.
    
    std::cout << "Verification: Checking final state of HalfAdder..." << std::endl;
    assert(pin_sum->getValue() == LogicValue::LOW && "Sum output was expected to be LOW.");
    assert(pin_carry->getValue() == LogicValue::HIGH && "Carry output was expected to be HIGH.");
}

size_t HalfAdderTest::getRunDuration() const {
    // Run long enough for the final (t=30) event to propagate through the circuit.
    return 50;
}