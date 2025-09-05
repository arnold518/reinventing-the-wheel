#include <iostream>
#include <memory>
#include <vector> // Required for std::vector
#include "simulator/Simulator.hpp"
#include "modules/Gate.hpp"
#include "modules/DFlipFlop.hpp"
#include "modules/ClockGenerator.hpp"
#include "components/ComponentBuilder.hpp"
#include "simulator/Event.hpp"
#include "basic/LogicValue.hpp"
#include "basic/Wire.hpp" // Needed for TestCircuit struct and setValue

// The struct to hold the final circuit and its external handles
struct TestCircuit {
    std::shared_ptr<Component> root;
    std::shared_ptr<ClockGenerator> clk_gen;
    std::shared_ptr<Wire> wire_a;
    std::shared_ptr<Wire> wire_b;
};

// The fully refactored build function using the unified addNewWire
TestCircuit buildTestCircuit() {
    auto root = std::make_shared<Component>("TEST1");
    ComponentBuilder builder(root);

    // 1. Create all components
    builder.addNewComponent<ANDGate>("AND1");
    builder.addNewComponent<DFlipFlop>("DFF1");
    builder.addNewComponent<ANDGate>("AND2");
    auto clk_gen = builder.addNewComponent<ClockGenerator>("CLK_GEN", 5);

    // 2. Create primary input wires and connect them in one step
    //    The builder creates the wire, connects it to all specified inputs,
    //    and returns a handle for us to use later.
    auto wire_a = builder.addNewWire("WireA", nullptr, {
        builder.getInputPin<ANDGate>("AND1", "A")
    });

    auto wire_b = builder.addNewWire("WireB", nullptr, {
        builder.getInputPin<ANDGate>("AND1", "B"),
        builder.getInputPin<ANDGate>("AND2", "B") // Fan-out to two components!
    });

    auto wire_gnd = builder.addNewWire("GND", nullptr, {
        builder.getInputPin<DFlipFlop>("DFF1", "RST")
    });
    wire_gnd->setValue(LogicValue::LOW);

    // 3. Create all internal and output-only wires in single, declarative calls
    builder.addNewWire("WireAND1_OUT",
        builder.getOutputPin<ANDGate>("AND1", "OUT"),
        { builder.getInputPin<DFlipFlop>("DFF1", "D") }
    );

    builder.addNewWire("WireCLK",
        builder.getOutputPin<ClockGenerator>("CLK_GEN", "CLK_OUT"),
        { builder.getInputPin<DFlipFlop>("DFF1", "CLK") }
    );

    builder.addNewWire("WireDFF1_Q",
        builder.getOutputPin<DFlipFlop>("DFF1", "Q"),
        { builder.getInputPin<ANDGate>("AND2", "A") }
    );

    // 4. Create output-only wires by providing an empty vector for the inputs
    builder.addNewWire("WireAND2_OUT", builder.getOutputPin<ANDGate>("AND2", "OUT"), {});
    builder.addNewWire("DFF1_Q_BAR_Wire", builder.getOutputPin<DFlipFlop>("DFF1", "Q_BAR"), {});

    // 5. Return the final circuit and the necessary handles
    return {root, clk_gen, wire_a, wire_b};
}

void setInitialState(Simulator& sim, const TestCircuit& circuit) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent>(0, circuit.wire_a, LogicValue::HIGH));
    sim.scheduleEvent(std::make_shared<WireUpdateEvent>(0, circuit.wire_b, LogicValue::LOW));
    circuit.clk_gen->startClock(sim, 0);
}

int main() {
    Simulator sim;
    TestCircuit circuit = buildTestCircuit();
    setInitialState(sim, circuit);

    sim.runAndRecord(100);

    std::cout << circuit.root->format(0, true, true) << "\n";

    auto V = sim.getUniqueTimestamps();
    std::cout << V.size() << "\n";
    return 0;
}