#include "modules/composite/ZeroDetect8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>

ZeroDetect8::ZeroDetect8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin("ZERO", PinType::OUTPUT);
    }) {}

void ZeroDetect8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<8>>("A_SPLIT");
    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<BitSplitter<8>, 8>("A_SPLIT", "IN")});

    for (size_t i = 0; i < 7; ++i) {
        builder.addNewComponent<ORGate>("OR" + std::to_string(i));
    }
    builder.addNewComponent<NOTGate>("NOT_ZERO");

    builder.addNewWire(
        "A_bit_0_to_OR0",
        builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("OR0", "A")});
    builder.addNewWire(
        "A_bit_1_to_OR0",
        builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("OR0", "B")});

    for (size_t i = 1; i < 7; ++i) {
        builder.addNewWire(
            "OR" + std::to_string(i - 1) + "_to_OR" + std::to_string(i),
            builder.getOutputPin<ORGate>("OR" + std::to_string(i - 1), "OUT"),
            {builder.getInputPin<ORGate>("OR" + std::to_string(i), "A")});
        builder.addNewWire(
            "A_bit_" + std::to_string(i + 1) + "_to_OR" + std::to_string(i),
            builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + std::to_string(i + 1)),
            {builder.getInputPin<ORGate>("OR" + std::to_string(i), "B")});
    }

    builder.addNewWire(
        "ANY_to_NOT",
        builder.getOutputPin<ORGate>("OR6", "OUT"),
        {builder.getInputPin<NOTGate>("NOT_ZERO", "IN")});
    builder.addNewWire(
        "NOT_to_ZERO",
        builder.getOutputPin<NOTGate>("NOT_ZERO", "OUT"),
        {getOutputPin("ZERO")});
}
