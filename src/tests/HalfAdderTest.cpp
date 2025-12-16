#include "tests/HalfAdderTest.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "modules/composite/HalfAdder.hpp"

std::string HalfAdderTest::getTestName() const {
    return "HalfAdderTest";
}

void HalfAdderTest::setupCircuit() {
    // Create a HalfAdder instance as the top-level component for this test
    root = Component::create<HalfAdder>("HA_ROOT");

    // Initialize the builder with this HalfAdder as the root context
    builder = std::make_unique<ComponentBuilder>(root);

    // buildCircuit() is empty (inherited from TruthTableTest)
    buildCircuit();

    // setInitialState() is auto-generated from truth table (inherited from TruthTableTest)
    setInitialState();
}

// Using new TruthTableTest API (Proposal 4)
std::vector<TruthRow> HalfAdderTest::getTruthTable() const {
    constexpr auto L = LogicValue::LOW;
    constexpr auto H = LogicValue::HIGH;

    // Half-Adder Truth Table:
    // A B | Sum Carry
    // 0 0 |  0   0
    // 0 1 |  1   0
    // 1 0 |  1   0
    // 1 1 |  0   1

    return {
        // Inputs              Outputs
        {{ {"A", L}, {"B", L} }, { {"Sum", L}, {"Carry", L} }},
        {{ {"A", L}, {"B", H} }, { {"Sum", H}, {"Carry", L} }},
        {{ {"A", H}, {"B", L} }, { {"Sum", H}, {"Carry", L} }},
        {{ {"A", H}, {"B", H} }, { {"Sum", L}, {"Carry", H} }},
    };
}
