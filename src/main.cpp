#include <iostream>
#include <memory>
#include "simulator/Simulator.hpp"
#include "modules/Gate.hpp"
#include "modules/DFlipFlop.hpp"
#include "modules/ClockGenerator.hpp"
#include "basic/Wire.hpp"
#include "basic/Pin.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Event.hpp"
#include "basic/LogicValue.hpp"

int main() {
    Simulator sim;

    auto and1 = IOComponent::create<ANDGate>("AND1");
    auto dff1 = IOComponent::create<DFlipFlop>("DFF1");
    auto and2 = IOComponent::create<ANDGate>("AND2");
    auto clk_gen = IOComponent::create<ClockGenerator>("CLK_GEN", 5);

    sim.addComponent(and1);
    sim.addComponent(dff1);
    sim.addComponent(and2);
    sim.addComponent(clk_gen);

    auto wire_a = std::make_shared<Wire>("WireA");
    auto wire_b = std::make_shared<Wire>("WireB");
    auto wire_and1_out = std::make_shared<Wire>("WireAND1_OUT");
    auto wire_dff1_q = std::make_shared<Wire>("WireDFF1_Q");
    auto wire_clk = std::make_shared<Wire>("WireCLK");
    auto wire_and2_out = std::make_shared<Wire>("WireAND2_OUT");
    auto wire_gnd = std::make_shared<Wire>("GND");
    wire_gnd->setValue(LogicValue::LOW);

    sim.addWire(wire_a);
    sim.addWire(wire_b);
    sim.addWire(wire_and1_out);
    sim.addWire(wire_dff1_q);
    sim.addWire(wire_clk);
    sim.addWire(wire_and2_out);
    sim.addWire(wire_gnd);

    and1->getInputPin("A")->connect(wire_a);
    and1->getInputPin("B")->connect(wire_b);
    and1->getOutputPin("OUT")->connect(wire_and1_out);

    dff1->getInputPin("D")->connect(wire_and1_out);
    dff1->getInputPin("CLK")->connect(wire_clk);
    dff1->getInputPin("RST")->connect(wire_gnd);
    dff1->getOutputPin("Q")->connect(wire_dff1_q);
    dff1->getOutputPin("Q_BAR")->connect(std::make_shared<Wire>("DFF1_Q_BAR_Wire"));

    and2->getInputPin("A")->connect(wire_dff1_q);
    and2->getInputPin("B")->connect(wire_b);
    and2->getOutputPin("OUT")->connect(wire_and2_out);

    clk_gen->getOutputPin("CLK_OUT")->connect(wire_clk);

    sim.scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_a, LogicValue::HIGH));
    sim.scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_b, LogicValue::LOW));
    clk_gen->startClock(sim, 0);

    sim.run(100);

    std::cout << "\nFinal Wire States:\n";
    std::cout << "WireA: " << wire_a->getValue() << std::endl;
    std::cout << "WireB: " << wire_b->getValue() << std::endl;
    std::cout << "WireAND1_OUT: " << wire_and1_out->getValue() << std::endl;
    std::cout << "WireDFF1_Q: " << wire_dff1_q->getValue() << std::endl;
    std::cout << "WireCLK: " << wire_clk->getValue() << std::endl;
    std::cout << "WireAND2_OUT: " << wire_and2_out->getValue() << std::endl;
    std::cout << "WireGND: " << wire_gnd->getValue() << std::endl;

    return 0;
}


/*
cd build
cmake ..
make
./sim
*/