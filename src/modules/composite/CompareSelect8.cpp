#include "modules/composite/CompareSelect8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

CompareSelect8::CompareSelect8(std::string name)
    : IOComponent(std::move(name), circuit::families::CompareSelect8.pinInitializer()) {}

void CompareSelect8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::Structural8Builder n(*this, builder);
    auto diff = n.split<8>("DIFF_SPLIT", n.input("DIFF"));
    auto zero = n.add(circuit::families::ZeroDetect8, "EQUAL");
    n.connect(n.input("DIFF"), zero->getInputPin<8>("A"));
    auto ltu = n.invert("UNSIGNED_LT", n.input("CARRY_IN"));
    auto lts = n.binary<XORGate>("SIGNED_LT", diff[7], n.input("OVERFLOW_IN"));
    auto lo = n.constant<1>("ZERO", 0);
    auto a = n.split<8>("A_SPLIT", n.input("A")), b = n.split<8>("B_SPLIT", n.input("B"));
    circuit::detail::Structural8Builder::Bits out;
    for (size_t i = 0; i < 8; ++i) {
        auto min_u = n.mux("MINU_" + std::to_string(i), b[i], a[i], ltu);
        auto max_u = n.mux("MAXU_" + std::to_string(i), a[i], b[i], ltu);
        auto min_s = n.mux("MINS_" + std::to_string(i), b[i], a[i], lts);
        auto max_s = n.mux("MAXS_" + std::to_string(i), a[i], b[i], lts);
        out[i] = n.selectBit8("RESULT_" + std::to_string(i),
            {i == 0 ? zero->getOutputPin("ZERO") : lo, i == 0 ? ltu : lo, i == 0 ? lts : lo,
             min_u, max_u, min_s, max_s, lo}, n.input("OP"));
    }
    n.connect(n.join<8>("OUT_JOIN", out), n.output("OUT"));
    n.finish();
}
