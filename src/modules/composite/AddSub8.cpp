#include "modules/composite/AddSub8.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

AddSub8::AddSub8(std::string name)
    : IOComponent(std::move(name), circuit::families::AddSub8.pinInitializer()) {}

void AddSub8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::Structural8Builder n(*this, builder);
    auto a = n.split<8>("A_SPLIT", n.input("A"));
    auto b = n.split<8>("B_SPLIT", n.input("B"));
    for (size_t i = 0; i < 8; ++i)
        b[i] = n.binary<XORGate>("B_XOR_SUB_" + std::to_string(i), b[i], n.input("SUB"));
    auto adder = n.add(circuit::families::Adder8, "ADD");
    n.connect(n.input("A"), adder->getInputPin<8>("A"));
    n.connect(n.join<8>("B_JOIN", b), adder->getInputPin<8>("B"));
    n.connect(n.input("C_IN"), adder->getInputPin("Cin"));
    auto result = adder->getOutputPin<8>("Sum");
    auto r = n.split<8>("RESULT_SPLIT", result);
    auto same_sign = n.invert("SAME_SIGN", n.binary<XORGate>("INPUT_SIGNS", a[7], b[7]));
    auto sign_changed = n.binary<XORGate>("RESULT_SIGN_CHANGED", a[7], r[7]);
    n.connect(n.binary<ANDGate>("OVERFLOW_AND", same_sign, sign_changed), n.output("OVERFLOW"));
    n.connect(result, n.output("OUT"));
    n.connect(adder->getOutputPin("Cout"), n.output("CARRY_OUT"));
    n.finish();
}
