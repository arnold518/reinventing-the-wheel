#include "modules/composite/LogicUnit8.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

LogicUnit8::LogicUnit8(std::string name)
    : IOComponent(std::move(name), circuit::families::LogicUnit8.pinInitializer()) {}

void LogicUnit8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::Structural8Builder n(*this, builder);
    auto a = n.split<8>("A_SPLIT", n.input("A")), b = n.split<8>("B_SPLIT", n.input("B"));
    auto zero = n.constant<1>("ZERO", 0);
    circuit::detail::Structural8Builder::Bits result;
    for (size_t i = 0; i < 8; ++i) {
        const auto suffix = "_" + std::to_string(i);
        auto and_bit = n.binary<ANDGate>("AND" + suffix, a[i], b[i]);
        auto or_bit = n.binary<ORGate>("OR" + suffix, a[i], b[i]);
        auto xor_bit = n.binary<XORGate>("XOR" + suffix, a[i], b[i]);
        result[i] = n.selectBit8("SELECT" + suffix,
            {and_bit, or_bit, xor_bit, n.invert("NOT" + suffix, a[i]),
             n.invert("NAND" + suffix, and_bit), n.invert("NOR" + suffix, or_bit),
             n.invert("XNOR" + suffix, xor_bit), zero}, n.input("OP"));
    }
    n.connect(n.join<8>("OUT_JOIN", result), n.output("OUT"));
    n.finish();
}
