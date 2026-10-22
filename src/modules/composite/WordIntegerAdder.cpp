#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBuilder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/WordDatapath.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/Rewire.hpp"
namespace circuit::word {
using OneBitPin=std::shared_ptr<Pin<>>;using SinkList=std::vector<OneBitPin>;
template<size_t W>void integerAdder(IOComponent& root,ComponentBuilder& builder,ALUConfig config) {
    circuit::detail::StructuralWiring w(builder);
    if (config.adder==AdderTopology::ParallelPrefix) {
        auto adder=builder.addNewComponent<ParallelPrefixAdder<W>>("PREFIX_ADDER");
        for (auto p:{"A","B","Cin"}) w.connect(root.getInputPinDynamic(p),adder->getInputPinDynamic(p));
        for (auto p:{"Sum","Cout"}) w.connect(adder->getOutputPinDynamic(p),root.getOutputPinDynamic(p));
        w.finish(); return;
    }
    builder.addNewComponent<BitSplitter<W>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<W>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<W>>("SUM_JOIN");

    w.connect(root.getInputPin<W>("A"),
        {builder.getInputPin<BitSplitter<W>, W>("A_SPLIT", "IN")});
    w.connect(root.getInputPin<W>("B"),
        {builder.getInputPin<BitSplitter<W>, W>("B_SPLIT", "IN")});

    for (size_t i = 0; i < W; ++i) {
        const auto index = std::to_string(i);
        const auto adder_name = "FA" + index;
        builder.addNewComponent<FullAdder>(adder_name);

        w.connect(builder.getOutputPin<BitSplitter<W>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        w.connect(builder.getOutputPin<BitSplitter<W>>("B_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        w.connect(builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<W>>("SUM_JOIN", "IN_" + index)});
    }

    w.connect(root.getInputPin("Cin"),
        {builder.getInputPin<FullAdder>("FA0", "Carry_in")});

    for (size_t i = 0; i < W-1; ++i) {
        w.connect(builder.getOutputPin<FullAdder>("FA" + std::to_string(i), "Carry_out"),
            {builder.getInputPin<FullAdder>("FA" + std::to_string(i + 1), "Carry_in")});
    }

    w.connect(builder.getOutputPin<FullAdder>("FA"+std::to_string(W-1), "Carry_out"),
        {root.getOutputPin("Cout")});

    w.connect(builder.getOutputPin<BitJoiner<W>, W>("SUM_JOIN", "OUT"),
        {root.getOutputPin<W>("Sum")});
    w.finish();
}
void buildIntegerAdder(IOComponent& root,ComponentBuilder& b,size_t w,ALUConfig c){
    if(w==8)integerAdder<8>(root,b,c);else if(w==16)integerAdder<16>(root,b,c);else integerAdder<32>(root,b,c);
}
}
