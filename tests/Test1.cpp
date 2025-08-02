#include "simulator/SimulationTest.hpp"
#include "modules/Gate.hpp"
#include "modules/DFlipFlop.hpp"
#include "modules/ClockGenerator.hpp"
#include "simulator/Event.hpp"
#include "basic/LogicValue.hpp"
#include "basic/Wire.hpp"
#include <cassert>

class FullCircuitIntegrationTest : public SimulationTest {
public:

    std::string getTestName() const override {
        return "Full_Circuit_Integration_Test";
    }

    void buildCircuit() override {
        // 1. Create all components
        builder->addNewComponent<ANDGate>("AND1");
        builder->addNewComponent<DFlipFlop>("DFF1");
        builder->addNewComponent<ANDGate>("AND2");
        auto clk_gen = builder->addNewComponent<ClockGenerator>("CLK_GEN", 5);

        // 2. Create primary input wires and connect them in one step
        //    The builder creates the wire, connects it to all specified inputs,
        //    and returns a handle for us to use later.
        auto wire_a = builder->addNewWire("WireA", nullptr, {
            builder->getInputPin<ANDGate>("AND1", "A")
        });

        auto wire_b = builder->addNewWire("WireB", nullptr, {
            builder->getInputPin<ANDGate>("AND1", "B"),
            builder->getInputPin<ANDGate>("AND2", "B") // Fan-out to two components!
        });

        auto wire_gnd = builder->addNewWire("GND", nullptr, {
            builder->getInputPin<DFlipFlop>("DFF1", "RST")
        });
        wire_gnd->setValue(LogicValue::LOW);

        // 3. Create all internal and output-only wires in single, declarative calls
        builder->addNewWire("WireAND1_OUT",
            builder->getOutputPin<ANDGate>("AND1", "OUT"),
            { builder->getInputPin<DFlipFlop>("DFF1", "D") }
        );

        builder->addNewWire("WireCLK",
            builder->getOutputPin<ClockGenerator>("CLK_GEN", "CLK_OUT"),
            { builder->getInputPin<DFlipFlop>("DFF1", "CLK") }
        );

        builder->addNewWire("WireDFF1_Q",
            builder->getOutputPin<DFlipFlop>("DFF1", "Q"),
            { builder->getInputPin<ANDGate>("AND2", "A") }
        );

        // 4. Create output-only wires by providing an empty vector for the inputs
        builder->addNewWire("WireAND2_OUT", builder->getOutputPin<ANDGate>("AND2", "OUT"), {});
        builder->addNewWire("DFF1_Q_BAR_Wire", builder->getOutputPin<DFlipFlop>("DFF1", "Q_BAR"), {});
    }

    void setInitialState() override {
        auto wire_a = builder->getWire("WireA");
        auto wire_b = builder->getWire("WireB");
        auto clk_gen = builder->getComponent<ClockGenerator>("CLK_GEN");
        
        sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_a, LogicValue::HIGH));
        sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_b, LogicValue::LOW));
        clk_gen->startClock(*sim, 0);
    }

    void verifyResults() override {
        auto dff_q_wire = builder->getWire("WireDFF1_Q");
        
        std::cout << "Verification: Checking final value of DFF1:Q..." << std::endl;
        assert(dff_q_wire->getValue() == LogicValue::LOW && "DFF output 'Q' was expected to be LOW but was HIGH.");
    }
    
    size_t getRunDuration() const override {
        return 100;
    }
};

int main() {
    FullCircuitIntegrationTest test;
    bool passed = test.run();
    
    // Return 0 for PASS, 1 for FAIL. This is the contract with CTest.
    return passed ? 0 : 1;
}