#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBuilder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/WordDatapath.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/Rewire.hpp"
namespace circuit::word {
using OneBitPin=std::shared_ptr<Pin<>>;using SinkList=std::vector<OneBitPin>;
template<size_t W>void integerShift(IOComponent& root,ComponentBuilder& builder,ALUConfig config) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<W>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<W>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<W>>("OUT_JOIN");
    builder.addNewComponent<NOTGate>("NOT_LEFT");
    builder.addNewComponent<ANDGate>("ARITHMETIC_SIGN");
    builder.addNewComponent<ANDGate>("FILL_ENABLE");

    w.connect(root.getInputPin<W>("A"),
        {builder.getInputPin<BitSplitter<W>, W>("A_SPLIT", "IN")});
    w.connect(root.getInputPin<W>("B"),
        {builder.getInputPin<BitSplitter<W>, W>("B_SPLIT", "IN")});
    w.connect(root.getInputPin("ARITHMETIC"),
        {builder.getInputPin<ANDGate>("ARITHMETIC_SIGN", "B")});
    w.connect(builder.getOutputPin<ANDGate>("ARITHMETIC_SIGN", "OUT"),
        {builder.getInputPin<ANDGate>("FILL_ENABLE", "A")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_LEFT", "OUT"),
        {builder.getInputPin<ANDGate>("FILL_ENABLE", "B")});

    // Reverse the input for SLL, feed one five-stage right barrel shifter,
    // then reverse the result back. This is the shared hardware resource.
    std::array<SinkList, W> a_sinks;
    a_sinks[W-1].push_back(
        builder.getInputPin<ANDGate>("ARITHMETIC_SIGN", "A"));
    std::vector<OneBitPin> current_sources;
    current_sources.reserve(W);
    std::vector<OneBitPin> left_select_sinks;
    left_select_sinks.reserve(2*W);
    left_select_sinks.push_back(
        builder.getInputPin<NOTGate>("NOT_LEFT", "IN"));
    for (size_t bit = 0; bit < W; ++bit) {
        const auto mux = "INPUT_REVERSE_" + std::to_string(bit);
        builder.addNewComponent<Mux<2,1>>(mux);
        a_sinks[bit].push_back(
            builder.getInputPin<Mux<2,1>>(mux, "A"));
        a_sinks[W-1-bit].push_back(
            builder.getInputPin<Mux<2,1>>(mux, "B"));
        left_select_sinks.push_back(
            builder.getInputPin<Mux<2,1>>(mux, "SEL"));
        current_sources.push_back(
            builder.getOutputPin<Mux<2,1>>(mux, "OUT"));
    }

    std::array<SinkList, std::countr_zero(W)> amount_select_sinks;
    SinkList fill_sinks;
    for (size_t stage = 0; stage < std::countr_zero(W); ++stage) {
        const size_t amount = size_t{1} << stage;
        std::array<SinkList, W> current_sinks;
        std::vector<OneBitPin> next_sources;
        next_sources.reserve(W);
        for (size_t bit = 0; bit < W; ++bit) {
            const auto mux = "SHIFT_S" + std::to_string(stage)
                + "_B" + std::to_string(bit);
            builder.addNewComponent<Mux<2,1>>(mux);
            current_sinks[bit].push_back(
                builder.getInputPin<Mux<2,1>>(mux, "A"));
            if (bit + amount < W) {
                current_sinks[bit + amount].push_back(
                    builder.getInputPin<Mux<2,1>>(mux, "B"));
            } else {
                fill_sinks.push_back(
                    builder.getInputPin<Mux<2,1>>(mux, "B"));
            }
            amount_select_sinks[stage].push_back(
                builder.getInputPin<Mux<2,1>>(mux, "SEL"));
            next_sources.push_back(
                builder.getOutputPin<Mux<2,1>>(mux, "OUT"));
        }
        for (size_t bit = 0; bit < W; ++bit) {
            w.connect(current_sources[bit],
                current_sinks[bit]);
        }
        current_sources = std::move(next_sources);
    }

    std::array<SinkList, W> output_sinks;
    for (size_t bit = 0; bit < W; ++bit) {
        const auto mux = "OUTPUT_REVERSE_" + std::to_string(bit);
        builder.addNewComponent<Mux<2,1>>(mux);
        output_sinks[bit].push_back(
            builder.getInputPin<Mux<2,1>>(mux, "A"));
        output_sinks[W-1-bit].push_back(
            builder.getInputPin<Mux<2,1>>(mux, "B"));
        left_select_sinks.push_back(
            builder.getInputPin<Mux<2,1>>(mux, "SEL"));
        w.connect(builder.getOutputPin<Mux<2,1>>(mux, "OUT"),
            {builder.getInputPin<BitJoiner<W>>(
                "OUT_JOIN", "IN_" + std::to_string(bit))});
    }
    for (size_t bit = 0; bit < W; ++bit) {
        w.connect(current_sources[bit],
            output_sinks[bit]);
    }

    for (size_t bit = 0; bit < W; ++bit) {
        w.connect(builder.getOutputPin<BitSplitter<W>>(
                "A_SPLIT", "OUT_" + std::to_string(bit)),
            a_sinks[bit]);
    }
    for (size_t bit = 0; bit < std::countr_zero(W); ++bit) {
        w.connect(builder.getOutputPin<BitSplitter<W>>(
                "B_SPLIT", "OUT_" + std::to_string(bit)),
            amount_select_sinks[bit]);
    }
    w.connect(root.getInputPin("LEFT"), left_select_sinks);
    w.connect(builder.getOutputPin<ANDGate>("FILL_ENABLE", "OUT"),
        fill_sinks);
    w.connect(builder.getOutputPin<BitJoiner<W>, W>("OUT_JOIN", "OUT"),
        {root.getOutputPin<W>("OUT")});
    w.finish();
}
void buildIntegerShift(IOComponent& root,ComponentBuilder& b,size_t w,ALUConfig c){
    if(w==8)integerShift<8>(root,b,c);else if(w==16)integerShift<16>(root,b,c);else integerShift<32>(root,b,c);
}
}
