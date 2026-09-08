#include "modules/composite/ArithmeticUnit8.hpp"
#include "modules/composite/AddSub8.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

ArithmeticUnit8::ArithmeticUnit8(std::string name)
    : IOComponent(std::move(name), circuit::families::ArithmeticUnit8.pinInitializer()) {}

void ArithmeticUnit8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::Structural8Builder n(*this, builder);
    auto lo = n.constant<1>("LOW", 0), hi = n.constant<1>("HIGH", 1);
    auto zero = n.constant<8>("ZERO8", 0), one = n.constant<8>("ONE8", 1);
    auto a = n.input("A"), b = n.input("B"), op = n.input("OP"), cin = n.input("C_IN");
    auto core = n.add(circuit::families::AddSub8, "ADD_SUB");
    n.connect(n.selectBus8("X_SELECT", {a,a,a,a,a,a,zero,zero}, op), core->getInputPin<8>("A"));
    n.connect(n.selectBus8("Y_SELECT", {b,b,b,b,zero,one,a,zero}, op), core->getInputPin<8>("B"));
    n.connect(n.selectBit8("SUB_SELECT", {lo,lo,hi,hi,lo,hi,hi,lo}, op), core->getInputPin("SUB"));
    n.connect(n.selectBit8("CARRY_IN_SELECT", {lo,cin,hi,cin,hi,hi,hi,lo}, op), core->getInputPin("C_IN"));
    auto invalid = n.selectBit8("INVALID_SELECT", {lo,lo,lo,lo,lo,lo,lo,hi}, op);
    auto valid = n.invert("VALID", invalid);
    n.connect(invalid, n.output("INVALID_OP"));
    n.connect(n.maskBus("RESULT_MASK", core->getOutputPin<8>("OUT"), valid), n.output("OUT"));
    n.connect(n.binary<ANDGate>("CARRY_MASK", core->getOutputPin("CARRY_OUT"), valid), n.output("CARRY"));
    n.connect(n.binary<ANDGate>("OVERFLOW_MASK", core->getOutputPin("OVERFLOW"), valid), n.output("OVERFLOW"));
    n.finish();
}
