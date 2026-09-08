#include "modules/composite/BarrelShifter8.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

BarrelShifter8::BarrelShifter8(std::string name)
    : IOComponent(std::move(name), circuit::families::BarrelShifter8.pinInitializer()) {}

void BarrelShifter8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::Structural8Builder n(*this, builder);
    auto a = n.split<8>("A_SPLIT", n.input("A")), b = n.split<8>("B_SPLIT", n.input("B"));
    auto lo = n.constant<1>("LOW", 0), hi = n.constant<1>("HIGH", 1), op = n.input("OP");
    auto left = n.selectBit8("LEFT_SELECT", {hi,lo,lo,hi,lo,lo,lo,lo}, op);
    auto rotate = n.selectBit8("ROTATE_SELECT", {lo,lo,lo,hi,hi,lo,lo,lo}, op);
    auto arithmetic = n.selectBit8("ARITHMETIC_SELECT", {lo,lo,hi,lo,lo,lo,lo,lo}, op);
    auto valid = n.selectBit8("VALID_SELECT", {hi,hi,hi,hi,hi,lo,lo,lo}, op);
    auto fill = n.binary<ANDGate>("SIGN_FILL", a[7], arithmetic);
    circuit::detail::Structural8Builder::Bits current;
    for (size_t i = 0; i < 8; ++i)
        current[i] = n.mux("INPUT_REVERSE_" + std::to_string(i), a[i], a[7-i], left);
    auto carry = n.input("C_IN");
    for (size_t stage = 0; stage < 3; ++stage) {
        const size_t distance = size_t{1} << stage;
        const auto prefix = "SHIFT_" + std::to_string(distance) + "_";
        auto previous = current;
        carry = n.mux(prefix + "CARRY", carry, previous[distance-1], b[stage]);
        for (size_t i = 0; i < 8; ++i) {
            auto moved = i + distance < 8 ? previous[i+distance]
                : n.mux(prefix + "FILL_" + std::to_string(i), fill, previous[i+distance-8], rotate);
            current[i] = n.mux(prefix + "BIT_" + std::to_string(i), previous[i], moved, b[stage]);
        }
    }
    auto shifted = current;
    for (size_t i = 0; i < 8; ++i) {
        current[i] = n.mux("OUTPUT_REVERSE_" + std::to_string(i), shifted[i], shifted[7-i], left);
        current[i] = n.binary<ANDGate>("VALID_BIT_" + std::to_string(i), current[i], valid);
    }
    n.connect(n.join<8>("OUT_JOIN", current), n.output("OUT"));
    n.connect(n.binary<ANDGate>("CARRY_VALID", carry, valid), n.output("CARRY"));
    n.finish();
}
