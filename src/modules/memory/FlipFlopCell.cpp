#include "modules/memory/FlipFlopCell.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/FlipFlopCellBehavioral.hpp"

namespace {
void defineFlipFlopCellPins(IOComponent* self) {
    self->addPin("D", PinType::INPUT);
    self->addPin("WE", PinType::INPUT);
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("Q", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily FlipFlopCell{
    "memory.ff.cell",
    "FlipFlopCell",
    defineFlipFlopCellPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::FlipFlopCell>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::FlipFlopCellBehavioral>(
            context, name);
    }};
}

FlipFlopCell::FlipFlopCell(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::FlipFlopCell.pinInitializer()) {}

void FlipFlopCell::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<Mux2to1>("WRITE_MUX");
    builder.addNewComponent<DFlipFlop>("STATE");

    w.connect(getInputPin("D"), builder.getInputPin<Mux2to1>("WRITE_MUX", "B"));

    w.connect(getInputPin("WE"), builder.getInputPin<Mux2to1>("WRITE_MUX", "SEL"));

    w.connect(getInputPin("CLK"), builder.getInputPin<DFlipFlop>("STATE", "CLK"));

    w.connect(getInputPin("RST"), builder.getInputPin<DFlipFlop>("STATE", "RST"));

    w.connect(builder.getOutputPin<Mux2to1>("WRITE_MUX", "OUT"), builder.getInputPin<DFlipFlop>("STATE", "D"));

    w.connect(builder.getOutputPin<DFlipFlop>("STATE", "Q"),
        {builder.getInputPin<Mux2to1>("WRITE_MUX", "A"),
        getOutputPin("Q")});
    w.finish();
}
