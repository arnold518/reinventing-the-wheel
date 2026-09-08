#include "modules/composite/ALU32CompareUnit.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

BEGIN_PINS(ALU32CompareUnit, IOComponent)
    INPUT_PIN_WIDTH("DIFF", 32)
    INPUT_PIN("CARRY_IN")
    INPUT_PIN("OVERFLOW_IN")
    INPUT_PIN("ENABLE")
    OUTPUT_PIN("EQ")
    OUTPUT_PIN("LT_SIGNED")
    OUTPUT_PIN("LT_UNSIGNED")
    OUTPUT_PIN_WIDTH("SLT_RESULT", 32)
    OUTPUT_PIN_WIDTH("SLTU_RESULT", 32)
END_PINS()

void ALU32CompareUnit::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralBuilder n(*this, builder);
    // Retain the selectable lower-level zero detector in CPU build profiles.
    auto zero = builder.add(circuit::families::ZeroDetect32, "ARITHMETIC_ZERO");
    n.connect(n.input("DIFF"), zero->getInputPin<32>("A"));
    auto diff = n.split<32>("ARITHMETIC_SPLIT", n.input("DIFF"));
    auto ltu = n.invert("NOT_CARRY", n.input("CARRY_IN"));
    auto lts = n.binary<XORGate>("SIGNED_LT_RAW", diff[31], n.input("OVERFLOW_IN"));
    auto enable = n.input("ENABLE");
    auto eq = n.binary<ANDGate>("EQ_VALID", zero->getOutputPin("ZERO"), enable);
    lts = n.binary<ANDGate>("SIGNED_LT_VALID", lts, enable);
    ltu = n.binary<ANDGate>("UNSIGNED_LT_VALID", ltu, enable);
    n.connect(eq, n.output("EQ"));
    n.connect(lts, n.output("LT_SIGNED"));
    n.connect(ltu, n.output("LT_UNSIGNED"));
    auto low = n.constant<1>("CONST_LOW", 0);
    std::array<circuit::detail::StructuralBuilder::Signal, 32> signed_word, unsigned_word;
    signed_word.fill(low); unsigned_word.fill(low);
    signed_word[0] = lts; unsigned_word[0] = ltu;
    n.connect(n.join<32>("SLT_JOIN", signed_word), n.output("SLT_RESULT"));
    n.connect(n.join<32>("SLTU_JOIN", unsigned_word), n.output("SLTU_RESULT"));
    n.finish();
}
