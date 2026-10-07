#include "modules/composite/ZeroDetect8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>

ZeroDetect8::ZeroDetect8(std::string name)
    : IOComponent(std::move(name), circuit::families::ZeroDetect8.pinInitializer()) {}

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

    for (size_t i = 0; i < 4; ++i) {
        for (size_t side = 0; side < 2; ++side)
            builder.addNewWire("A_bit_" + std::to_string(2 * i + side) + "_to_OR" + std::to_string(i),
                builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + std::to_string(2 * i + side)),
                {builder.getInputPin<ORGate>("OR" + std::to_string(i), side == 0 ? "A" : "B")});
    }
    for (size_t i = 4; i < 7; ++i) {
        for (size_t side = 0; side < 2; ++side) {
            const auto child = i == 6 ? 4 + side : 2 * (i - 4) + side;
            builder.addNewWire("OR" + std::to_string(child) + "_to_OR" + std::to_string(i),
                builder.getOutputPin<ORGate>("OR" + std::to_string(child), "OUT"),
                {builder.getInputPin<ORGate>("OR" + std::to_string(i), side == 0 ? "A" : "B")});
        }
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
