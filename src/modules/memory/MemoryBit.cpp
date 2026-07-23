#include "modules/memory/MemoryBit.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/WireBuilder.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/MemoryBitDirect.hpp"

namespace {
void defineMemoryBitPins(IOComponent* self) {
    self->addPin("D", PinType::INPUT);
    self->addPin("WE", PinType::INPUT);
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("Q", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily MemoryBit{
    "memory.write-enabled-bit",
    "MemoryBit",
    defineMemoryBitPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::MemoryBit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::MemoryBitDirect>(
            context, name);
    }};
}

MemoryBit::MemoryBit(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::MemoryBit.pinInitializer()) {}

void MemoryBit::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Mux2to1>("WRITE_MUX");
    builder.addNewComponent<DFlipFlop>("STATE");

    builder.wire("D_to_write_mux")
        .fromInput("D")
        .to<Mux2to1>("WRITE_MUX", "B");

    builder.wire("WE_to_write_mux")
        .fromInput("WE")
        .to<Mux2to1>("WRITE_MUX", "SEL");

    builder.wire("CLK_to_state")
        .fromInput("CLK")
        .to<DFlipFlop>("STATE", "CLK");

    builder.wire("RST_to_state")
        .fromInput("RST")
        .to<DFlipFlop>("STATE", "RST");

    builder.wire("write_mux_to_state")
        .from<Mux2to1>("WRITE_MUX", "OUT")
        .to<DFlipFlop>("STATE", "D");

    builder.wire("state_feedback")
        .from<DFlipFlop>("STATE", "Q")
        .to<Mux2to1>("WRITE_MUX", "A")
        .toOutput("Q");
}
