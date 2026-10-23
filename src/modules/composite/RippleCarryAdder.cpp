#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/WordRecipes.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
namespace circuit::word {
// W full adders: each cell's Carry_out feeds the next cell's Carry_in.
template<size_t W>void rippleAdder(IOComponent& root,ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
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

    w.connect(root.getInputPin("C_IN"),
        {builder.getInputPin<FullAdder>("FA0", "Carry_in")});

    for (size_t i = 0; i < W-1; ++i) {
        w.connect(builder.getOutputPin<FullAdder>("FA" + std::to_string(i), "Carry_out"),
            {builder.getInputPin<FullAdder>("FA" + std::to_string(i + 1), "Carry_in")});
    }

    w.connect(builder.getOutputPin<FullAdder>("FA"+std::to_string(W-1), "Carry_out"),
        {root.getOutputPin("C_OUT")});

    w.connect(builder.getOutputPin<BitJoiner<W>, W>("SUM_JOIN", "OUT"),
        {root.getOutputPin<W>("OUT")});
    w.finish();
}
void buildRippleAdder(IOComponent& root,ComponentBuilder& b,size_t w){
    if(w==8)rippleAdder<8>(root,b);else if(w==16)rippleAdder<16>(root,b);else rippleAdder<32>(root,b);
}
}
