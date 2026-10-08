#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Logic8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>

namespace {
template<typename GateT>
void buildBinaryLogic8(IOComponent& component, ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<8>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<8>>("OUT_JOIN");

    w.connect(component.getInputPin<8>("A"),
        {builder.getInputPin<BitSplitter<8>, 8>("A_SPLIT", "IN")});
    w.connect(component.getInputPin<8>("B"),
        {builder.getInputPin<BitSplitter<8>, 8>("B_SPLIT", "IN")});

    for (size_t i = 0; i < 8; ++i) {
        const auto index = std::to_string(i);
        const auto gate_name = "G" + index;
        builder.addNewComponent<GateT>(gate_name);
        w.connect(builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<GateT>(gate_name, "A")});
        w.connect(builder.getOutputPin<BitSplitter<8>>("B_SPLIT", "OUT_" + index),
            {builder.getInputPin<GateT>(gate_name, "B")});
        w.connect(builder.getOutputPin<GateT>(gate_name, "OUT"),
            {builder.getInputPin<BitJoiner<8>>("OUT_JOIN", "IN_" + index)});
    }

    w.connect(builder.getOutputPin<BitJoiner<8>, 8>("OUT_JOIN", "OUT"),
        {component.getOutputPin<8>("OUT")});
    w.finish();
}

void buildNot8(IOComponent& component, ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<8>>("A_SPLIT");
    builder.addNewComponent<BitJoiner<8>>("OUT_JOIN");

    w.connect(component.getInputPin<8>("A"),
        {builder.getInputPin<BitSplitter<8>, 8>("A_SPLIT", "IN")});

    for (size_t i = 0; i < 8; ++i) {
        const auto index = std::to_string(i);
        const auto gate_name = "G" + index;
        builder.addNewComponent<NOTGate>(gate_name);
        w.connect(builder.getOutputPin<BitSplitter<8>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<NOTGate>(gate_name, "IN")});
        w.connect(builder.getOutputPin<NOTGate>(gate_name, "OUT"),
            {builder.getInputPin<BitJoiner<8>>("OUT_JOIN", "IN_" + index)});
    }

    w.connect(builder.getOutputPin<BitJoiner<8>, 8>("OUT_JOIN", "OUT"),
        {component.getOutputPin<8>("OUT")});
    w.finish();
}
}

XOR8::XOR8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void XOR8::buildInternals(ComponentBuilder& builder) {
    buildBinaryLogic8<XORGate>(*this, builder);
}

NOT8::NOT8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void NOT8::buildInternals(ComponentBuilder& builder) {
    buildNot8(*this, builder);
}
