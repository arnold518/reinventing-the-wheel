#include "modules/composite/ALU32FlagUnit.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "modules/utility/Rewire.hpp"
#include "components/PinMacros.hpp"

BEGIN_PINS(ALU32FlagUnit, IOComponent)
    INPUT_PIN_WIDTH("RESULT", 32)
    INPUT_PIN("ARITHMETIC_ENABLE")
    INPUT_PIN("ARITH_CARRY")
    INPUT_PIN("ARITH_OVERFLOW")
    OUTPUT_PIN("ZERO")
    OUTPUT_PIN("NEGATIVE")
    OUTPUT_PIN("CARRY_OUT")
    OUTPUT_PIN("OVERFLOW")
END_PINS()

void ALU32FlagUnit::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralBuilder n(*this, builder);
    auto zero = builder.add(circuit::families::ZeroDetect32, "RESULT_ZERO");
    n.connect(n.input("RESULT"), zero->getInputPin<32>("A"));
    n.connect(zero->getOutputPin("ZERO"), n.output("ZERO"));
    // Preserve ALU32's direct sign-bit forwarding, including four-state values.
    auto sign = n.add<Rewire>("SIGN_BIT",
        std::vector<Rewire::WireSpec>{{"RESULT", 32}},
        std::vector<Rewire::WireSpec>{{"SIGN", 1}},
        std::vector<Rewire::BitMap>{{"RESULT", 31, "SIGN", 0}});
    n.connect(n.input("RESULT"), sign->getInputPin<32>("RESULT"));
    n.connect(sign->getOutputPin("SIGN"), n.output("NEGATIVE"));
    n.connect(n.binary<ANDGate>("CARRY_VALID", n.input("ARITH_CARRY"), n.input("ARITHMETIC_ENABLE")), n.output("CARRY_OUT"));
    n.connect(n.binary<ANDGate>("OVERFLOW_VALID", n.input("ARITH_OVERFLOW"), n.input("ARITHMETIC_ENABLE")), n.output("OVERFLOW"));
    n.finish();
}
