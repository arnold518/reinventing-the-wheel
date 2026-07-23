#include "modules/memory/RegisterFile32x32.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/Register32BitCellArray.hpp"
#include "modules/memory/Register32.hpp"
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

    for (size_t reg = 1; reg < RegisterCount; ++reg) {
        builder.add(circuit::families::Register32, regName(reg));
    }

    std::vector<std::shared_ptr<Pin<32>>> write_data_sinks;
    std::vector<std::shared_ptr<Pin<>>> clk_sinks;
    std::vector<std::shared_ptr<Pin<>>> rst_sinks;
    write_data_sinks.reserve(RegisterCount);
    clk_sinks.reserve(RegisterCount - 1);
    rst_sinks.reserve(RegisterCount - 1);

    for (size_t reg = 1; reg < RegisterCount; ++reg) {
        const auto name = regName(reg);
        write_data_sinks.push_back(builder.getInputPin<IOComponent, 32>(name, "D"));
        clk_sinks.push_back(builder.getInputPin<IOComponent>(name, "CLK"));
        rst_sinks.push_back(builder.getInputPin<IOComponent>(name, "RST"));
    }

    builder.addNewWire<32>("WRITE_DATA_internal", getInputPin<32>("WRITE_DATA"), write_data_sinks);
    builder.addNewWire("CLK_internal", getInputPin("CLK"), clk_sinks);
    builder.addNewWire("RST_internal", getInputPin("RST"), rst_sinks);

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
            {builder.getInputPin<IOComponent>(regName(reg), "WE")});
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
            builder.getOutputPin<IOComponent, 32>(name, "Q"),
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
        const auto& name = child->getName();
        if (name.size() < 2 || name.front() != 'X') continue;
        const auto index = static_cast<size_t>(std::stoul(name.substr(1)));
        if (index == 0 || index >= result.size()) continue;
        const auto word = std::dynamic_pointer_cast<IOComponent>(child);
        if (word) result[index] = word->getOutputPin<32>("Q")->getValueAsVector();
    }
    return result;
}
