#include "modules/composite/Adder8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"

Adder8::Adder8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("Cin", PinType::INPUT);
        self->addPin<8>("Sum", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
    }) {}

void Adder8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<8>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<8>>("SUM_JOIN");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<BitSplitter<8>, 8>("A_SPLIT", "IN")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<BitSplitter<8>, 8>("B_SPLIT", "IN")});

    for (size_t i = 0; i < 8; ++i) {
        const auto index = std::to_string(i);
        const auto adder_name = "FA" + index;
        builder.addNewComponent<FullAdder>(adder_name);

        builder.addNewWire(
            "A_bit_" + index,
            builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        builder.addNewWire(
            "B_bit_" + index,
            builder.getOutputPin<BitSplitter<8>>("B_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        builder.addNewWire(
            "SUM_bit_" + index,
            builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<8>>("SUM_JOIN", "IN_" + index)});
    }

    builder.addNewWire(
        "carry_in",
        getInputPin("Cin"),
        {builder.getInputPin<FullAdder>("FA0", "Carry_in")});

    for (size_t i = 0; i < 7; ++i) {
        builder.addNewWire(
            "carry_" + std::to_string(i) + "_to_" + std::to_string(i + 1),
            builder.getOutputPin<FullAdder>("FA" + std::to_string(i), "Carry_out"),
            {builder.getInputPin<FullAdder>("FA" + std::to_string(i + 1), "Carry_in")});
    }

    builder.addNewWire(
        "carry_out",
        builder.getOutputPin<FullAdder>("FA7", "Carry_out"),
        {getOutputPin("Cout")});

    builder.addNewWire<8>(
        "sum_bus_internal",
        builder.getOutputPin<BitJoiner<8>, 8>("SUM_JOIN", "OUT"),
        {getOutputPin<8>("Sum")});
}
