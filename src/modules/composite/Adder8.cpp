#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/Adder8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"

Adder8::Adder8(std::string name)
    : IOComponent(std::move(name), circuit::families::Adder8.pinInitializer()) {}

void Adder8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<8>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<8>>("SUM_JOIN");

    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<BitSplitter<8>, 8>("A_SPLIT", "IN")});
    w.connect(getInputPin<8>("B"),
        {builder.getInputPin<BitSplitter<8>, 8>("B_SPLIT", "IN")});

    for (size_t i = 0; i < 8; ++i) {
        const auto index = std::to_string(i);
        const auto adder_name = "FA" + index;
        builder.addNewComponent<FullAdder>(adder_name);

        w.connect(builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        w.connect(builder.getOutputPin<BitSplitter<8>>("B_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        w.connect(builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<8>>("SUM_JOIN", "IN_" + index)});
    }

    w.connect(getInputPin("Cin"),
        {builder.getInputPin<FullAdder>("FA0", "Carry_in")});

    for (size_t i = 0; i < 7; ++i) {
        w.connect(builder.getOutputPin<FullAdder>("FA" + std::to_string(i), "Carry_out"),
            {builder.getInputPin<FullAdder>("FA" + std::to_string(i + 1), "Carry_in")});
    }

    w.connect(builder.getOutputPin<FullAdder>("FA7", "Carry_out"),
        {getOutputPin("Cout")});

    w.connect(builder.getOutputPin<BitJoiner<8>, 8>("SUM_JOIN", "OUT"),
        {getOutputPin<8>("Sum")});
    w.finish();
}
