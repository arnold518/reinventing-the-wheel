#include "modules/composite/Adder32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>

Adder32::Adder32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
          self->addPin<32>("A", PinType::INPUT);
          self->addPin<32>("B", PinType::INPUT);
          self->addPin("Cin", PinType::INPUT);
          self->addPin<32>("Sum", PinType::OUTPUT);
          self->addPin("Cout", PinType::OUTPUT);
      }) {}

void Adder32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("SUM_JOIN");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});

    for (size_t i = 0; i < 32; ++i) {
        const auto index = std::to_string(i);
        const auto adder_name = "FA" + index;
        builder.addNewComponent<FullAdder>(adder_name);

        builder.addNewWire(
            "A_bit_" + index,
            builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        builder.addNewWire(
            "B_bit_" + index,
            builder.getOutputPin<BitSplitter<32>>("B_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        builder.addNewWire(
            "SUM_bit_" + index,
            builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<32>>("SUM_JOIN", "IN_" + index)});
    }

    builder.addNewWire(
        "carry_in",
        getInputPin("Cin"),
        {builder.getInputPin<FullAdder>("FA0", "Carry_in")});

    for (size_t i = 0; i < 31; ++i) {
        builder.addNewWire(
            "carry_" + std::to_string(i) + "_to_" + std::to_string(i + 1),
            builder.getOutputPin<FullAdder>("FA" + std::to_string(i), "Carry_out"),
            {builder.getInputPin<FullAdder>("FA" + std::to_string(i + 1), "Carry_in")});
    }

    builder.addNewWire(
        "carry_out",
        builder.getOutputPin<FullAdder>("FA31", "Carry_out"),
        {getOutputPin("Cout")});

    builder.addNewWire<32>(
        "sum_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("SUM_JOIN", "OUT"),
        {getOutputPin<32>("Sum")});
}
