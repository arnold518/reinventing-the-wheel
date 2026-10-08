#include "modules/storage/ClockedStateBit.hpp"
#include "modules/storage/ClockedStateBitBehavioral.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/DFlipFlop.hpp"

using circuit::BuildContext;
using circuit::ComponentFamily;

namespace {
void defineClockedStateBitPins(IOComponent* self) {
    for (const auto* pin : {"D", "CLK", "RST"}) self->addPin(pin, PinType::INPUT);
    self->addPin("Q", PinType::OUTPUT);
}
}
ClockedStateBit::ClockedStateBit(std::string name)
    : IOComponent(std::move(name), circuit::families::ClockedStateBit.pinInitializer()) {}
void ClockedStateBit::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring wires(builder);
    auto state = builder.addNewComponent<DFlipFlop>("STATE");
    for (const auto* pin : {"D", "CLK", "RST"})
        wires.connect(getInputPin(pin), state->getInputPin(pin));
    wires.connect(state->getOutputPin("Q"), getOutputPin("Q"));
    wires.finish();
}
ClockedStateBitBehavioral::ClockedStateBitBehavioral(std::string name)
    : BasicComponent(std::move(name), 1, circuit::families::ClockedStateBit.pinInitializer()) {}
void ClockedStateBitBehavioral::evaluate(size_t time, Simulator& simulator) {
    const auto reset = getInputValue("RST");
    const auto clock = getInputValue("CLK");
    if (reset == LogicValue::HIGH) stored_ = LogicValue::LOW;
    else if (reset != LogicValue::LOW) stored_ = stored_ == LogicValue::LOW ? LogicValue::LOW : LogicValue::UNKNOWN;
    else if (previous_clock_ == LogicValue::LOW && clock == LogicValue::HIGH) {
        const auto data = getInputValue("D");
        stored_ = data == LogicValue::HIGH || data == LogicValue::LOW ? data : LogicValue::UNKNOWN;
    }
    previous_clock_ = clock;
    _updateOutputWire(simulator, "Q", stored_, time);
}
namespace circuit::families {
const ComponentFamily ClockedStateBit{
    "storage.state-bit", "ClockedStateBit", defineClockedStateBitPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::ClockedStateBit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<ClockedStateBitBehavioral>(context, name);
    }};
}
