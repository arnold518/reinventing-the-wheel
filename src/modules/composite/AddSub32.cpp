#include "modules/composite/AddSub32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>
#include <vector>

AddSub32::AddSub32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
          self->addPin<32>("A", PinType::INPUT);
          self->addPin<32>("B", PinType::INPUT);
          self->addPin("SUB", PinType::INPUT);
          self->addPin<32>("OUT", PinType::OUTPUT);
          self->addPin("CARRY_OUT", PinType::OUTPUT);
          self->addPin("OVERFLOW", PinType::OUTPUT);
      }) {}

void AddSub32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("OUT_JOIN");
    builder.addNewComponent<XORGate>("OVERFLOW_XOR");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});

    std::vector<std::shared_ptr<Pin<>>> sub_sinks;
    sub_sinks.reserve(33);

    for (size_t i = 0; i < 32; ++i) {
        const auto bit = std::to_string(i);
        const auto xor_name = "B_XOR_SUB_" + bit;
        const auto adder_name = "FA" + bit;

        builder.addNewComponent<XORGate>(xor_name);
        builder.addNewComponent<FullAdder>(adder_name);

        sub_sinks.push_back(builder.getInputPin<XORGate>(xor_name, "B"));

        builder.addNewWire(
            "A_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + bit),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        builder.addNewWire(
            "B_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("B_SPLIT", "OUT_" + bit),
            {builder.getInputPin<XORGate>(xor_name, "A")});
        builder.addNewWire(
            "B_xor_SUB_" + bit + "_to_FA",
            builder.getOutputPin<XORGate>(xor_name, "OUT"),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        builder.addNewWire(
            "SUM_bit_" + bit,
            builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<32>>("OUT_JOIN", "IN_" + bit)});
    }

    sub_sinks.push_back(builder.getInputPin<FullAdder>("FA0", "Carry_in"));
    builder.addNewWire("SUB_control", getInputPin("SUB"), sub_sinks);

    for (size_t i = 0; i < 31; ++i) {
        const auto from = std::to_string(i);
        const auto to = std::to_string(i + 1);
        std::vector<std::shared_ptr<Pin<>>> sinks{
            builder.getInputPin<FullAdder>("FA" + to, "Carry_in")};
        if (i == 30) {
            sinks.push_back(builder.getInputPin<XORGate>("OVERFLOW_XOR", "A"));
        }
        builder.addNewWire(
            "carry_" + from + "_to_" + to,
            builder.getOutputPin<FullAdder>("FA" + from, "Carry_out"),
            sinks);
    }

    builder.addNewWire(
        "carry_out_fanout",
        builder.getOutputPin<FullAdder>("FA31", "Carry_out"),
        {getOutputPin("CARRY_OUT"),
         builder.getInputPin<XORGate>("OVERFLOW_XOR", "B")});
    builder.addNewWire(
        "overflow_to_output",
        builder.getOutputPin<XORGate>("OVERFLOW_XOR", "OUT"),
        {getOutputPin("OVERFLOW")});
    builder.addNewWire<32>(
        "out_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("OUT_JOIN", "OUT"),
        {getOutputPin<32>("OUT")});
}
