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
template<size_t W>void integerLogic(IOComponent& root,ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<W>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<W>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<W>>("AND_JOIN");
    builder.addNewComponent<BitJoiner<W>>("OR_JOIN");
    builder.addNewComponent<BitJoiner<W>>("XOR_JOIN");

    w.connect(root.getInputPin<W>("A"),
        {builder.getInputPin<BitSplitter<W>, W>("A_SPLIT", "IN")});
    w.connect(root.getInputPin<W>("B"),
        {builder.getInputPin<BitSplitter<W>, W>("B_SPLIT", "IN")});

    for (size_t i = 0; i < W; ++i) {
        const auto bit = std::to_string(i);
        const auto and_name = "AND_" + bit;
        const auto or_name = "OR_" + bit;
        const auto xor_name = "XOR_" + bit;

        builder.addNewComponent<ANDGate>(and_name);
        builder.addNewComponent<ORGate>(or_name);
        builder.addNewComponent<XORGate>(xor_name);

        w.connect(builder.getOutputPin<BitSplitter<W>>("A_SPLIT", "OUT_" + bit),
            {builder.getInputPin<ANDGate>(and_name, "A"),
             builder.getInputPin<ORGate>(or_name, "A"),
             builder.getInputPin<XORGate>(xor_name, "A")});
        w.connect(builder.getOutputPin<BitSplitter<W>>("B_SPLIT", "OUT_" + bit),
            {builder.getInputPin<ANDGate>(and_name, "B"),
             builder.getInputPin<ORGate>(or_name, "B"),
             builder.getInputPin<XORGate>(xor_name, "B")});
        w.connect(builder.getOutputPin<ANDGate>(and_name, "OUT"),
            {builder.getInputPin<BitJoiner<W>>("AND_JOIN", "IN_" + bit)});
        w.connect(builder.getOutputPin<ORGate>(or_name, "OUT"),
            {builder.getInputPin<BitJoiner<W>>("OR_JOIN", "IN_" + bit)});
        w.connect(builder.getOutputPin<XORGate>(xor_name, "OUT"),
            {builder.getInputPin<BitJoiner<W>>("XOR_JOIN", "IN_" + bit)});
    }

    w.connect(builder.getOutputPin<BitJoiner<W>, W>("AND_JOIN", "OUT"),
        {root.getOutputPin<W>("AND_OUT")});
    w.connect(builder.getOutputPin<BitJoiner<W>, W>("OR_JOIN", "OUT"),
        {root.getOutputPin<W>("OR_OUT")});
    w.connect(builder.getOutputPin<BitJoiner<W>, W>("XOR_JOIN", "OUT"),
        {root.getOutputPin<W>("XOR_OUT")});
    w.finish();
}
}
void RV32IALULogic::buildInternals(ComponentBuilder& b) { rv32i::alu_detail::integerLogic<32>(*this,b); }
