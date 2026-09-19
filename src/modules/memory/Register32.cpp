#include "modules/memory/Register32.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/memory/FFWord.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/memory/Register32Direct.hpp"

namespace {
void defineRegister32Pins(IOComponent* self) {
    self->addPin<32>("D", PinType::INPUT);
    self->addPin("WE", PinType::INPUT);
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin<32>("Q", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily Register32{
    "memory.register.width32",
    "Register32",
    defineRegister32Pins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::Register32>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::Register32Direct>(
            context, name);
    }};
}

Register32::Register32(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::Register32.pinInitializer()) {}

void Register32::buildInternals(ComponentBuilder& builder) {
    // Same reusable storage word as the architectural register file. All
    // bytes are enabled here: this public interface writes a whole register.
    const std::shared_ptr<IOComponent> storage = builder.getBuildContext()
        ? builder.add(circuit::families::FFWord<32>(), "STORAGE")
        : builder.addNewComponent<FFWord<32>>("STORAGE");
    auto mask = builder.addNewComponent<ConstantValue<4>>("FULL_WRITE_MASK", 15);
    circuit::memory_detail::StructuralWiring wires(builder);
    wires.connect(getInputPin<32>("D"), storage->getInputPin<32>("WRITE_DATA"));
    wires.connect(mask->getOutputPin<4>("OUT"), storage->getInputPin<4>("WRITE_MASK"));
    for (const auto* pin : {"WE", "CLK", "RST"})
        wires.connect(getInputPin(pin), storage->getInputPin(pin));
    wires.connect(storage->getOutputPin<32>("Q"), getOutputPin<32>("Q"));
    wires.finish();
}
