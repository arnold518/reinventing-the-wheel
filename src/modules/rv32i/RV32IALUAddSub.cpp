#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"

// SUB inverts B and supplies the carry-in, so A - B = A + ~B + 1. Overflow is
// the carry into the sign bit XOR the carry out of it.
void RV32IALUAddSub::buildInternals(ComponentBuilder& builder) {
    constexpr size_t W = 32;
    circuit::detail::StructuralWiring w(builder);
    auto bits=builder.addNewComponent<BitSplitter<W>>("B_SPLIT");
    auto effective=builder.addNewComponent<BitJoiner<W>>("EFFECTIVE_B");
    auto adder=builder.addNewComponent<ParallelPrefixAdder<W>>("PREFIX_ADDER");
    w.connect(getInputPin<W>("B"),bits->getInputPin<W>("IN"));
    for (size_t i=0;i<W;++i) {
        const auto n=std::to_string(i);
        w.connect(w.binary<XORGate>("B_XOR_SUB_"+n,bits->getOutputPin("OUT_"+n),getInputPin("SUB")),effective->getInputPin("IN_"+n));
    }
    w.connect(getInputPin<W>("A"),adder->getInputPin<W>("A"));
    w.connect(effective->getOutputPin<W>("OUT"),adder->getInputPin<W>("B"));
    w.connect(getInputPin("SUB"),adder->getInputPin("C_IN"));
    w.connect(adder->getOutputPin<W>("OUT"),getOutputPin<W>("OUT"));
    w.connect(adder->getOutputPin("C_OUT"),getOutputPin("C_OUT"));
    w.connect(w.binary<XORGate>("OVERFLOW_XOR",adder->getOutputPin("C_SIGN"),adder->getOutputPin("C_OUT")),getOutputPin("OVERFLOW"));
    w.finish();
}
