#include "modules/memory/Register32.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/WireBuilder.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32Direct.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>
#include <vector>

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
    builder.addNewComponent<BitSplitter<32>>("D_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("Q_JOIN");

    builder.addNewWire<32>(
        "D_bus_internal",
        getInputPin<32>("D"),
        {builder.getInputPin<BitSplitter<32>, 32>("D_SPLIT", "IN")});

    std::vector<std::shared_ptr<Pin<>>> we_sinks;
    std::vector<std::shared_ptr<Pin<>>> clk_sinks;
    std::vector<std::shared_ptr<Pin<>>> rst_sinks;
    we_sinks.reserve(32);
    clk_sinks.reserve(32);
    rst_sinks.reserve(32);

    for (size_t bit = 0; bit < 32; ++bit) {
        const auto bit_text = std::to_string(bit);
        const auto cell_name = "BIT_" + bit_text;
        builder.add(circuit::families::MemoryBit, cell_name);

        builder.addNewWire(
            "D_bit_" + bit_text,
            builder.getOutputPin<BitSplitter<32>>("D_SPLIT", "OUT_" + bit_text),
            {builder.getInputPin<IOComponent>(cell_name, "D")});

        builder.addNewWire(
            "Q_bit_" + bit_text,
            builder.getOutputPin<IOComponent>(cell_name, "Q"),
            {builder.getInputPin<BitJoiner<32>>("Q_JOIN", "IN_" + bit_text)});

        we_sinks.push_back(builder.getInputPin<IOComponent>(cell_name, "WE"));
        clk_sinks.push_back(builder.getInputPin<IOComponent>(cell_name, "CLK"));
        rst_sinks.push_back(builder.getInputPin<IOComponent>(cell_name, "RST"));
    }

    builder.addNewWire("WE_internal", getInputPin("WE"), we_sinks);
    builder.addNewWire("CLK_internal", getInputPin("CLK"), clk_sinks);
    builder.addNewWire("RST_internal", getInputPin("RST"), rst_sinks);

    builder.addNewWire<32>(
        "Q_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("Q_JOIN", "OUT"),
        {getOutputPin<32>("Q")});
}
