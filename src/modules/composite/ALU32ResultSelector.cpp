#include "modules/composite/ALU32ResultSelector.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

BEGIN_PINS(ALU32ResultSelector, IOComponent)
    INPUT_PIN_WIDTH("ARITHMETIC_RESULT", 32)
    INPUT_PIN_WIDTH("AND_RESULT", 32)
    INPUT_PIN_WIDTH("OR_RESULT", 32)
    INPUT_PIN_WIDTH("XOR_RESULT", 32)
    INPUT_PIN_WIDTH("SHIFT_RESULT", 32)
    INPUT_PIN_WIDTH("SLT_RESULT", 32)
    INPUT_PIN_WIDTH("SLTU_RESULT", 32)
    INPUT_PIN_WIDTH("A", 32)
    INPUT_PIN_WIDTH("B", 32)
    INPUT_PIN_WIDTH("LOCAL_OP", 3)
    INPUT_PIN("HIGH_HALF")
    INPUT_PIN("OUT_OF_RANGE")
    OUTPUT_PIN_WIDTH("OUT", 32)
END_PINS()

void ALU32ResultSelector::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralBuilder n(*this, builder);
    auto low = n.add<Mux8to1_32bit>("RESULT_LOW_MUX");
    auto high = n.add<Mux8to1_32bit>("RESULT_HIGH_MUX");
    auto half = n.add<Mux2to1_32bit>("RESULT_HALF_MUX");
    auto range = n.add<Mux2to1_32bit>("RESULT_RANGE_MUX");
    auto zero = n.constant<32>("CONST_ZERO32", 0);
    const std::array<circuit::detail::StructuralBuilder::Signal, 8> low_sources{
        n.input("ARITHMETIC_RESULT"), n.input("ARITHMETIC_RESULT"),
        n.input("AND_RESULT"), n.input("OR_RESULT"), n.input("XOR_RESULT"),
        n.input("SHIFT_RESULT"), n.input("SHIFT_RESULT"), n.input("SHIFT_RESULT")};
    const std::array<circuit::detail::StructuralBuilder::Signal, 8> high_sources{
        n.input("SLT_RESULT"), n.input("SLTU_RESULT"), n.input("A"), n.input("B"),
        zero, zero, zero, zero};
    for (size_t i = 0; i < 8; ++i) {
        n.connect(low_sources[i], low->getInputPin<32>("IN" + std::to_string(i)));
        n.connect(high_sources[i], high->getInputPin<32>("IN" + std::to_string(i)));
    }
    n.connect(n.input("LOCAL_OP"), low->getInputPin<3>("SEL"));
    n.connect(n.input("LOCAL_OP"), high->getInputPin<3>("SEL"));
    n.connect(low->getOutputPin<32>("OUT"), half->getInputPin<32>("A"));
    n.connect(high->getOutputPin<32>("OUT"), half->getInputPin<32>("B"));
    n.connect(n.input("HIGH_HALF"), half->getInputPin("SEL"));
    n.connect(half->getOutputPin<32>("OUT"), range->getInputPin<32>("A"));
    n.connect(zero, range->getInputPin<32>("B"));
    n.connect(n.input("OUT_OF_RANGE"), range->getInputPin("SEL"));
    n.connect(range->getOutputPin<32>("OUT"), n.output("OUT"));
    n.finish();
}
