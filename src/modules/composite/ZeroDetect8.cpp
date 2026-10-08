#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>

ZeroDetect8::ZeroDetect8(std::string name)
    : IOComponent(std::move(name), circuit::families::ZeroDetect8.pinInitializer()) {}

void ZeroDetect8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<8>>("A_SPLIT");
    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<BitSplitter<8>, 8>("A_SPLIT", "IN")});

    for (size_t i = 0; i < 7; ++i) {
        builder.addNewComponent<ORGate>("OR" + std::to_string(i));
    }
    builder.addNewComponent<NOTGate>("NOT_ZERO");

    for (size_t i = 0; i < 4; ++i) {
        for (size_t side = 0; side < 2; ++side)
            w.connect(builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + std::to_string(2 * i + side)),
                {builder.getInputPin<ORGate>("OR" + std::to_string(i), side == 0 ? "A" : "B")});
    }
    for (size_t i = 4; i < 7; ++i) {
        for (size_t side = 0; side < 2; ++side) {
            const auto child = i == 6 ? 4 + side : 2 * (i - 4) + side;
            w.connect(builder.getOutputPin<ORGate>("OR" + std::to_string(child), "OUT"),
                {builder.getInputPin<ORGate>("OR" + std::to_string(i), side == 0 ? "A" : "B")});
        }
    }

    w.connect(builder.getOutputPin<ORGate>("OR6", "OUT"),
        {builder.getInputPin<NOTGate>("NOT_ZERO", "IN")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_ZERO", "OUT"),
        {getOutputPin("ZERO")});
    w.finish();
}
