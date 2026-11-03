#include "components/ComponentBuilder.tpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "modules/composite/WordSliceBuilder.hpp"
#include <bit>
namespace rv32i::alu_detail {
// Reverse the input for SLL, feed one five-stage right barrel shifter, then
// reverse the result back. Each step is one 2:1 word mux (one Mux<2,1> per
// bit); reversing and moving a word is wiring only.
template<size_t W>void integerShift(IOComponent& root,ComponentBuilder& builder) {
    using Signal = circuit::detail::WordSliceBuilder::Signal;
    circuit::detail::WordSliceBuilder n(root, builder);
    const auto a = n.split<W>("A_SPLIT", n.input("A"));
    const auto b = n.split<W>("B_SPLIT", n.input("B"));
    const auto left = n.input("LEFT");
    const auto sign = n.binary<ANDGate>("ARITHMETIC_SIGN", a[W-1], n.input("ARITHMETIC"));
    const auto fill = n.binary<ANDGate>("FILL_ENABLE", sign, n.invert("NOT_LEFT", left));

    const auto reversed = [&](const std::string& name, const std::array<Signal, W>& bits) {
        std::array<Signal, W> r;
        for (size_t i = 0; i < W; ++i) r[i] = bits[W-1-i];
        return n.join<W>(name, r);
    };
    auto current = n.wordMux("INPUT_REVERSE", n.input("A"), reversed("A_REVERSED", a), left);
    for (size_t stage = 0; stage < std::countr_zero(W); ++stage) {
        const size_t amount = size_t{1} << stage;
        const auto prefix = "SHIFT_S" + std::to_string(stage);
        const auto bits = n.split<W>(prefix + "_SPLIT", current);
        std::array<Signal, W> moved;
        for (size_t i = 0; i < W; ++i) moved[i] = i + amount < W ? bits[i + amount] : fill;
        current = n.wordMux(prefix, current, n.join<W>(prefix + "_MOVED", moved), b[stage]);
    }
    const auto shifted = n.split<W>("SHIFTED_SPLIT", current);
    n.connect(n.wordMux("OUTPUT_REVERSE", current, reversed("SHIFTED_REVERSED", shifted), left), n.output("OUT"));
    n.finish();
}
}
void RV32IALUShifter::buildInternals(ComponentBuilder& b) { rv32i::alu_detail::integerShift<32>(*this,b); }
