#include "components/ComponentBuilder.tpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "modules/composite/WordSliceBuilder.hpp"
#include "modules/composite/ZeroDetect.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "components/StructuralNetBuilder.hpp"
namespace rv32i::alu_detail {
using OneBitPin=std::shared_ptr<Pin<>>;using SinkList=std::vector<OneBitPin>;
template<size_t W>void integerCompare(IOComponent& root,ComponentBuilder& builder) {
    circuit::detail::WordSliceBuilder n(root, builder);
    std::shared_ptr<IOComponent> zero = builder.addNewComponent<ZeroDetect<W>>("ARITHMETIC_ZERO");
    n.connect(n.input("DIFF"), zero->template getInputPin<W>("A"));
    auto diff = n.split<W>("ARITHMETIC_SPLIT", n.input("DIFF"));
    auto ltu = n.invert("NOT_CARRY", n.input("CARRY_IN"));
    auto lts = n.binary<XORGate>("SIGNED_LT_RAW", diff[W-1], n.input("OVERFLOW_IN"));
    auto enable = n.input("ENABLE");
    auto eq = n.binary<ANDGate>("EQ_VALID", zero->getOutputPin("ZERO"), enable);
    lts = n.binary<ANDGate>("SIGNED_LT_VALID", lts, enable);
    ltu = n.binary<ANDGate>("UNSIGNED_LT_VALID", ltu, enable);
    n.connect(eq, n.output("EQ"));
    n.connect(lts, n.output("LT_SIGNED"));
    n.connect(ltu, n.output("LT_UNSIGNED"));
    auto low = n.constant<1>("CONST_LOW", 0);
    std::array<circuit::detail::WordSliceBuilder::Signal, W> signed_word, unsigned_word;
    signed_word.fill(low); unsigned_word.fill(low);
    signed_word[0] = lts; unsigned_word[0] = ltu;
    n.connect(n.join<W>("SLT_JOIN", signed_word), n.output("SLT_RESULT"));
    n.connect(n.join<W>("SLTU_JOIN", unsigned_word), n.output("SLTU_RESULT"));
    n.finish();
}
}
void RV32IALUCompare::buildInternals(ComponentBuilder& b) { rv32i::alu_detail::integerCompare<32>(*this,b); }
