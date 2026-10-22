#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/WordDatapath.hpp"
#include "modules/utility/BitAdapter.hpp"
namespace circuit::word {
namespace {
void zeroLegacy8(IOComponent& root,ComponentBuilder& builder){
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<8>>("A_SPLIT");
    w.connect(root.getInputPin<8>("A"),
        {builder.getInputPin<BitSplitter<8>, 8>("A_SPLIT", "IN")});

    for (size_t i = 0; i < 7; ++i) {
        builder.addNewComponent<ORGate>("OR" + std::to_string(i));
    }
    builder.addNewComponent<NOTGate>("NOT_ZERO");

    for (size_t i = 0; i < 4; ++i) {
        for (size_t side = 0; side < 2; ++side)
            w.connect(builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + std::to_string(2 * i + side)),
                {builder.getInputPin<ORGate>("OR" + std::to_string(i), side == 0 ? "A" : "B")});
    }
    for (size_t i = 4; i < 7; ++i) {
        for (size_t side = 0; side < 2; ++side) {
            const auto child = i == 6 ? 4 + side : 2 * (i - 4) + side;
            w.connect(builder.getOutputPin<ORGate>("OR" + std::to_string(child), "OUT"),
                {builder.getInputPin<ORGate>("OR" + std::to_string(i), side == 0 ? "A" : "B")});
        }
    }

    w.connect(builder.getOutputPin<ORGate>("OR6", "OUT"),
        {builder.getInputPin<NOTGate>("NOT_ZERO", "IN")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_ZERO", "OUT"),
        {root.getOutputPin("ZERO")});
    w.finish();
}
template<size_t W>void zeroWord(IOComponent& root,ComponentBuilder& builder){
    circuit::detail::StructuralWiring w(builder);
    auto bits=builder.addNewComponent<BitSplitter<W>>("A_SPLIT");
    w.connect(root.getInputPin<W>("A"),bits->template getInputPin<W>("IN"));
    std::vector<circuit::detail::StructuralWiring::PinPtr> terms;
    for(size_t i=0;i<W;++i) terms.push_back(bits->getOutputPin("OUT_"+std::to_string(i)));
    w.connect(w.invert("NOT_ANY",w.reduce("ANY_BIT",terms,false)),root.getOutputPin("ZERO"));
    w.finish();
}
}
void buildWordZero(IOComponent& root,ComponentBuilder& b,size_t w,ALUConfig){if(w==8)zeroLegacy8(root,b);else if(w==16)zeroWord<16>(root,b);else zeroWord<32>(root,b);}
}
