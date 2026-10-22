#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBuilder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/WordDatapath.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/Rewire.hpp"
namespace circuit::word {
using OneBitPin=std::shared_ptr<Pin<>>;using SinkList=std::vector<OneBitPin>;
template<size_t W>void integerAddSub(IOComponent& root,ComponentBuilder& builder,ALUConfig config) {
    circuit::detail::StructuralWiring w(builder);
    if (config.adder==AdderTopology::ParallelPrefix) {
        auto bits=builder.addNewComponent<BitSplitter<W>>("B_SPLIT");
        auto effective=builder.addNewComponent<BitJoiner<W>>("EFFECTIVE_B");
        auto adder=builder.addNewComponent<ParallelPrefixAdder<W>>("PREFIX_ADDER");
        w.connect(root.getInputPin<W>("B"),bits->template getInputPin<W>("IN"));
        for (size_t i=0;i<W;++i) {
            const auto n=std::to_string(i);
            w.connect(w.binary<XORGate>("B_XOR_SUB_"+n,bits->getOutputPin("OUT_"+n),root.getInputPin("SUB")),effective->getInputPin("IN_"+n));
        }
        w.connect(root.getInputPin<W>("A"),adder->template getInputPin<W>("A"));
        w.connect(effective->template getOutputPin<W>("OUT"),adder->template getInputPin<W>("B"));
        w.connect(root.getInputPin("SUB"),adder->getInputPin("Cin"));
        w.connect(adder->template getOutputPin<W>("Sum"),root.getOutputPin<W>("OUT"));
        w.connect(adder->getOutputPin("Cout"),root.getOutputPin("CARRY_OUT"));
        w.connect(w.binary<XORGate>("OVERFLOW_XOR",adder->getOutputPin("C"+std::to_string(W-1)),adder->getOutputPin("Cout")),root.getOutputPin("OVERFLOW"));
        w.finish(); return;
    }
    builder.addNewComponent<BitSplitter<W>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<W>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<W>>("OUT_JOIN");
    builder.addNewComponent<XORGate>("OVERFLOW_XOR");

    w.connect(root.getInputPin<W>("A"),
        {builder.getInputPin<BitSplitter<W>, W>("A_SPLIT", "IN")});
    w.connect(root.getInputPin<W>("B"),
        {builder.getInputPin<BitSplitter<W>, W>("B_SPLIT", "IN")});

    std::vector<std::shared_ptr<Pin<>>> sub_sinks;
    sub_sinks.reserve(W + 1);

    for (size_t i = 0; i < W; ++i) {
        const auto bit = std::to_string(i);
        const auto xor_name = "B_XOR_SUB_" + bit;
        const auto adder_name = "FA" + bit;

        builder.addNewComponent<XORGate>(xor_name);
        builder.addNewComponent<FullAdder>(adder_name);

        sub_sinks.push_back(builder.getInputPin<XORGate>(xor_name, "B"));

        w.connect(builder.getOutputPin<BitSplitter<W>>("A_SPLIT", "OUT_" + bit),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        w.connect(builder.getOutputPin<BitSplitter<W>>("B_SPLIT", "OUT_" + bit),
            {builder.getInputPin<XORGate>(xor_name, "A")});
        w.connect(builder.getOutputPin<XORGate>(xor_name, "OUT"),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        w.connect(builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<W>>("OUT_JOIN", "IN_" + bit)});
    }

    sub_sinks.push_back(builder.getInputPin<FullAdder>("FA0", "Carry_in"));
    w.connect(root.getInputPin("SUB"), sub_sinks);

    for (size_t i = 0; i < W-1; ++i) {
        const auto from = std::to_string(i);
        const auto to = std::to_string(i + 1);
        std::vector<std::shared_ptr<Pin<>>> sinks{
            builder.getInputPin<FullAdder>("FA" + to, "Carry_in")};
        if (i == W-2) {
            sinks.push_back(builder.getInputPin<XORGate>("OVERFLOW_XOR", "A"));
        }
        w.connect(builder.getOutputPin<FullAdder>("FA" + from, "Carry_out"),
            sinks);
    }

    w.connect(builder.getOutputPin<FullAdder>("FA"+std::to_string(W-1), "Carry_out"),
        {root.getOutputPin("CARRY_OUT"),
         builder.getInputPin<XORGate>("OVERFLOW_XOR", "B")});
    w.connect(builder.getOutputPin<XORGate>("OVERFLOW_XOR", "OUT"),
        {root.getOutputPin("OVERFLOW")});
    w.connect(builder.getOutputPin<BitJoiner<W>, W>("OUT_JOIN", "OUT"),
        {root.getOutputPin<W>("OUT")});
    w.finish();
}
void buildIntegerAddSub(IOComponent& root,ComponentBuilder& b,size_t w,ALUConfig c){
    if(w==8)integerAddSub<8>(root,b,c);else if(w==16)integerAddSub<16>(root,b,c);else integerAddSub<32>(root,b,c);
}
}
