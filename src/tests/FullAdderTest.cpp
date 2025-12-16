#include "tests/FullAdderTest.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "modules/composite/FullAdder.hpp"

std::string FullAdderTest::getTestName() const {
    return "FullAdderTest";
}

void FullAdderTest::setupCircuit() {
    // Create a FullAdder instance as the top-level component for this test
    root = Component::create<FullAdder>("FA_ROOT");

    // Initialize the builder with this FullAdder as the root context
    builder = std::make_unique<ComponentBuilder>(root);

    // buildCircuit() is empty (inherited from TruthTableTest)
    buildCircuit();

    // setInitialState() is auto-generated from truth table (inherited from TruthTableTest)
    setInitialState();
}

// Using new TruthTableTest API (Proposal 4)
std::vector<TruthRow> FullAdderTest::getTruthTable() const {
    constexpr auto L = LogicValue::LOW;
    constexpr auto H = LogicValue::HIGH;

    // Full-Adder Truth Table:
    // A B Cin | Sum Cout
    // 0 0  0  |  0   0
    // 0 0  1  |  1   0
    // 0 1  0  |  1   0
    // 0 1  1  |  0   1
    // 1 0  0  |  1   0
    // 1 0  1  |  0   1
    // 1 1  0  |  0   1
    // 1 1  1  |  1   1

    return {
        // Inputs                                    Outputs
        {{ {"A", L}, {"B", L}, {"Carry_in", L} }, { {"Sum", L}, {"Carry_out", L} }},
        {{ {"A", L}, {"B", L}, {"Carry_in", H} }, { {"Sum", H}, {"Carry_out", L} }},
        {{ {"A", L}, {"B", H}, {"Carry_in", L} }, { {"Sum", H}, {"Carry_out", L} }},
        {{ {"A", L}, {"B", H}, {"Carry_in", H} }, { {"Sum", L}, {"Carry_out", H} }},
        {{ {"A", H}, {"B", L}, {"Carry_in", L} }, { {"Sum", H}, {"Carry_out", L} }},
        {{ {"A", H}, {"B", L}, {"Carry_in", H} }, { {"Sum", L}, {"Carry_out", H} }},
        {{ {"A", H}, {"B", H}, {"Carry_in", L} }, { {"Sum", L}, {"Carry_out", H} }},
        {{ {"A", H}, {"B", H}, {"Carry_in", H} }, { {"Sum", H}, {"Carry_out", H} }},
    };
}
