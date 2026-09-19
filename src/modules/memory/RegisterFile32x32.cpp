#include "modules/memory/RegisterFile32x32.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/FFStorageBank.hpp"
#include "modules/memory/RegisterFile32x32Direct.hpp"
#include "modules/utility/Constant.hpp"
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr size_t RegisterCount = 32;
constexpr size_t AddressWidth = 5;

std::string regName(size_t index) {
    return "X" + std::to_string(index);
}

void defineRegisterFile32x32Pins(IOComponent* self) {
    self->addPin<5>("RS1_ADDR", PinType::INPUT);
    self->addPin<5>("RS2_ADDR", PinType::INPUT);
    self->addPin<5>("RD_ADDR", PinType::INPUT);
    self->addPin<32>("WRITE_DATA", PinType::INPUT);
    self->addPin("REG_WRITE", PinType::INPUT);
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin<32>("RS1_DATA", PinType::OUTPUT);
    self->addPin<32>("RS2_DATA", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily RegisterFile32x32{
    "rv32i.register-file",
    "RegisterFile32x32",
    defineRegisterFile32x32Pins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RegisterFile32x32>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RegisterFile32x32Direct>(
            context, name);
    }};
}

RegisterFile32x32::RegisterFile32x32(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::RegisterFile32x32.pinInitializer()) {}

void RegisterFile32x32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<ConstantValue<32>>("X0_ZERO", 0);
    builder.addNewComponent<Mux32to1_32bit>("RS1_MUX");
    builder.addNewComponent<Mux32to1_32bit>("RS2_MUX");
    builder.addNewComponent<Decoder5to32>("RD_DECODER");

    // x0 has no storage. Physical word n holds architectural register x(n+1).
    // Keep decoding/read selection outside the reusable parallel-output FF bank.
    const std::shared_ptr<IOComponent> storage = builder.getBuildContext()
        ? builder.add(circuit::families::FFStorageBank<32>(), "STORAGE", {{"depth_words", "31"}})
        : builder.addNewComponent<FFStorageBank<32>>("STORAGE", RegisterCount - 1);
    auto mask = builder.addNewComponent<ConstantValue<4>>("FULL_WRITE_MASK", 15);
    circuit::memory_detail::StructuralWiring storage_wires(builder);
    for (const auto* pin : {"WRITE_DATA", "CLK", "RST"})
        storage_wires.connect(getInputPinDynamic(pin), storage->getInputPinDynamic(pin));
    storage_wires.connect(mask->getOutputPin<4>("OUT"), storage->getInputPin<4>("WRITE_MASK"));
    storage_wires.finish();

    builder.addNewWire<AddressWidth>(
        "RD_ADDR_internal",
        getInputPin<AddressWidth>("RD_ADDR"),
        {builder.getInputPin<Decoder5to32, AddressWidth>("RD_DECODER", "ADDR")});

    builder.addNewWire(
        "REG_WRITE_internal",
        getInputPin("REG_WRITE"),
        {builder.getInputPin<Decoder5to32>("RD_DECODER", "ENABLE")});

    for (size_t reg = 1; reg < RegisterCount; ++reg) {
        builder.addNewWire(
            regName(reg) + "_WE_internal",
            builder.getOutputPin<Decoder5to32>("RD_DECODER", "OUT" + std::to_string(reg)),
            {storage->getInputPin("WRITE_" + std::to_string(reg - 1))});
    }

    builder.addNewWire<AddressWidth>(
        "RS1_ADDR_internal",
        getInputPin<AddressWidth>("RS1_ADDR"),
        {builder.getInputPin<Mux32to1_32bit, AddressWidth>("RS1_MUX", "SEL")});

    builder.addNewWire<AddressWidth>(
        "RS2_ADDR_internal",
        getInputPin<AddressWidth>("RS2_ADDR"),
        {builder.getInputPin<Mux32to1_32bit, AddressWidth>("RS2_MUX", "SEL")});

    builder.addNewWire<32>(
        "X0_to_read_muxes",
        builder.getOutputPin<ConstantValue<32>, 32>("X0_ZERO", "OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RS1_MUX", "IN0"),
         builder.getInputPin<Mux32to1_32bit, 32>("RS2_MUX", "IN0")});

    for (size_t reg = 1; reg < RegisterCount; ++reg) {
        const auto name = regName(reg);
        const auto input_name = "IN" + std::to_string(reg);
        builder.addNewWire<32>(
            name + "_to_read_muxes",
            storage->getOutputPin<32>("WORD_" + std::to_string(reg - 1)),
            {builder.getInputPin<Mux32to1_32bit, 32>("RS1_MUX", input_name),
             builder.getInputPin<Mux32to1_32bit, 32>("RS2_MUX", input_name)});
    }

    builder.addNewWire<32>(
        "RS1_DATA_internal",
        builder.getOutputPin<Mux32to1_32bit, 32>("RS1_MUX", "OUT"),
        {getOutputPin<32>("RS1_DATA")});

    builder.addNewWire<32>(
        "RS2_DATA_internal",
        builder.getOutputPin<Mux32to1_32bit, 32>("RS2_MUX", "OUT"),
        {getOutputPin<32>("RS2_DATA")});
}

std::vector<std::vector<LogicValue>> RegisterFile32x32::getRegisterStateAtTime(
    size_t target_time) const {
    (void)target_time;
    std::vector<std::vector<LogicValue>> result(
        RegisterCount, std::vector<LogicValue>(32, LogicValue::LOW));
    for (const auto& child : getChildren()) {
        if (child->getName() != "STORAGE") continue;
        const auto storage = std::dynamic_pointer_cast<IOComponent>(child);
        if (storage) for (size_t reg = 1; reg < RegisterCount; ++reg)
            result[reg] = storage->getOutputPin<32>("WORD_" + std::to_string(reg - 1))->getValueAsVector();
    }
    return result;
}
