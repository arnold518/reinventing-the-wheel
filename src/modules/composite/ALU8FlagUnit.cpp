#include "modules/composite/ALU8FlagUnit.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

ALU8FlagUnit::ALU8FlagUnit(std::string name)
    : IOComponent(std::move(name), circuit::families::ALU8FlagUnit.pinInitializer()) {}

void ALU8FlagUnit::buildInternals(ComponentBuilder& builder) {
    circuit::detail::Structural8Builder n(*this, builder);
    auto zero = n.add(circuit::families::ZeroDetect8, "ZERO_DETECT");
    n.connect(n.input("RESULT"), zero->getInputPin<8>("A"));
    n.connect(zero->getOutputPin("ZERO"), n.output("ZERO"));
    auto result = n.split<8>("RESULT_SPLIT", n.input("RESULT"));
    auto low = n.constant<1>("LOW", 0);
    n.connect(n.binary<ORGate>("SIGN", result[7], low), n.output("NEGATIVE"));
    auto arithmetic_carry = n.binary<ANDGate>("ARITH_CARRY_MASK", n.input("ARITHMETIC"), n.input("ARITH_CARRY"));
    auto shift_carry = n.binary<ANDGate>("SHIFT_CARRY_MASK", n.input("SHIFT"), n.input("SHIFT_CARRY"));
    n.connect(n.binary<ORGate>("CARRY_SELECT", arithmetic_carry, shift_carry), n.output("CARRY"));
    n.connect(n.binary<ANDGate>("OVERFLOW_SELECT", n.input("ARITHMETIC"), n.input("ARITH_OVERFLOW")), n.output("OVERFLOW"));
    n.finish();
}
