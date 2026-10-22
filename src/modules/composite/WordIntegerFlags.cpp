#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBuilder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/WordDatapath.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/Rewire.hpp"
namespace circuit::word {
using OneBitPin=std::shared_ptr<Pin<>>;using SinkList=std::vector<OneBitPin>;
template<size_t W>void integerFlags(IOComponent& root,ComponentBuilder& builder,ALUConfig config) {
    circuit::detail::WordSliceBuilder n(root, builder);
    std::shared_ptr<IOComponent> zero = builder.addNewComponent<ZeroDetect<W>>("RESULT_ZERO");
    n.connect(n.input("RESULT"), zero->template getInputPin<W>("A"));
    n.connect(zero->getOutputPin("ZERO"), n.output("ZERO"));
    // Forward the selected word's sign bit directly, including four-state values.
    auto sign = n.add<Rewire>("SIGN_BIT",
        std::vector<Rewire::WireSpec>{{"RESULT", W}},
        std::vector<Rewire::WireSpec>{{"SIGN", 1}},
        std::vector<Rewire::BitMap>{{"RESULT", W-1, "SIGN", 0}});
    n.connect(n.input("RESULT"), sign->template getInputPin<W>("RESULT"));
    n.connect(sign->getOutputPin("SIGN"), n.output("NEGATIVE"));
    n.connect(n.binary<ANDGate>("CARRY_VALID", n.input("ARITH_CARRY"), n.input("ARITHMETIC_ENABLE")), n.output("CARRY_OUT"));
    n.connect(n.binary<ANDGate>("OVERFLOW_VALID", n.input("ARITH_OVERFLOW"), n.input("ARITHMETIC_ENABLE")), n.output("OVERFLOW"));
    n.finish();
}
void buildIntegerFlags(IOComponent& root,ComponentBuilder& b,size_t w,ALUConfig c){
    if(w==8)integerFlags<8>(root,b,c);else if(w==16)integerFlags<16>(root,b,c);else integerFlags<32>(root,b,c);
}
}
