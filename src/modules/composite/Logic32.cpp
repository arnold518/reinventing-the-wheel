#include "modules/composite/Logic32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>

Logic32::Logic32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
          self->addPin<32>("A", PinType::INPUT);
          self->addPin<32>("B", PinType::INPUT);
          self->addPin<32>("AND_OUT", PinType::OUTPUT);
          self->addPin<32>("OR_OUT", PinType::OUTPUT);
          self->addPin<32>("XOR_OUT", PinType::OUTPUT);
      }) {}

void Logic32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("AND_JOIN");
    builder.addNewComponent<BitJoiner<32>>("OR_JOIN");
    builder.addNewComponent<BitJoiner<32>>("XOR_JOIN");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});

    for (size_t i = 0; i < 32; ++i) {
        const auto bit = std::to_string(i);
        const auto and_name = "AND_" + bit;
        const auto or_name = "OR_" + bit;
        const auto xor_name = "XOR_" + bit;

        builder.addNewComponent<ANDGate>(and_name);
        builder.addNewComponent<ORGate>(or_name);
        builder.addNewComponent<XORGate>(xor_name);

        builder.addNewWire(
            "A_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + bit),
            {builder.getInputPin<ANDGate>(and_name, "A"),
             builder.getInputPin<ORGate>(or_name, "A"),
             builder.getInputPin<XORGate>(xor_name, "A")});
        builder.addNewWire(
            "B_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("B_SPLIT", "OUT_" + bit),
            {builder.getInputPin<ANDGate>(and_name, "B"),
             builder.getInputPin<ORGate>(or_name, "B"),
             builder.getInputPin<XORGate>(xor_name, "B")});
        builder.addNewWire(
            "AND_bit_" + bit,
            builder.getOutputPin<ANDGate>(and_name, "OUT"),
            {builder.getInputPin<BitJoiner<32>>("AND_JOIN", "IN_" + bit)});
        builder.addNewWire(
            "OR_bit_" + bit,
            builder.getOutputPin<ORGate>(or_name, "OUT"),
            {builder.getInputPin<BitJoiner<32>>("OR_JOIN", "IN_" + bit)});
        builder.addNewWire(
            "XOR_bit_" + bit,
            builder.getOutputPin<XORGate>(xor_name, "OUT"),
            {builder.getInputPin<BitJoiner<32>>("XOR_JOIN", "IN_" + bit)});
    }

    builder.addNewWire<32>(
        "AND_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("AND_JOIN", "OUT"),
        {getOutputPin<32>("AND_OUT")});
    builder.addNewWire<32>(
        "OR_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("OR_JOIN", "OUT"),
        {getOutputPin<32>("OR_OUT")});
    builder.addNewWire<32>(
        "XOR_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("XOR_JOIN", "OUT"),
        {getOutputPin<32>("XOR_OUT")});
}
