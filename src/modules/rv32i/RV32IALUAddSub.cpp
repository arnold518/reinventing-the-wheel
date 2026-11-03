#include "components/ComponentBuilder.tpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/WordSliceBuilder.hpp"

// SUB inverts B and supplies the carry-in, so A - B = A + ~B + 1. Overflow is
// the carry into the sign bit XOR the carry out of it.
void RV32IALUAddSub::buildInternals(ComponentBuilder& builder) {
    constexpr size_t W = 32;
    circuit::detail::WordSliceBuilder n(*this, builder);
    // B XOR SUB on every bit: one word XOR with SUB spread across the word.
    const auto effective_b = n.wordGate(circuit::word::GateKind::Xor, "B_XOR_SUB", n.input("B"),
                                        n.broadcast("SUB_WORD", n.input("SUB"), W));
    auto adder = n.add<ParallelPrefixAdder<W>>("PREFIX_ADDER");
    n.connect(n.input("A"), adder->getInputPinDynamic("A"));
    n.connect(effective_b, adder->getInputPinDynamic("B"));
    n.connect(n.input("SUB"), adder->getInputPinDynamic("C_IN"));
    n.connect(adder->getOutputPinDynamic("OUT"), n.output("OUT"));
    n.connect(adder->getOutputPinDynamic("C_OUT"), n.output("C_OUT"));
    n.connect(n.binary<XORGate>("OVERFLOW_XOR", adder->getOutputPinDynamic("C_SIGN"), adder->getOutputPinDynamic("C_OUT")),
              n.output("OVERFLOW"));
    n.finish();
}
