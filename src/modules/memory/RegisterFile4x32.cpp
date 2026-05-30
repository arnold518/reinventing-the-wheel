#include "modules/memory/RegisterFile4x32.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/WireBuilder.hpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/BehavioralRegister32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <memory>
#include <string>
#include <vector>

BEGIN_PINS(RegisterFile4x32, IOComponent)
    INPUT_PIN_WIDTH("RS1_ADDR", 2)
    INPUT_PIN_WIDTH("RS2_ADDR", 2)
    INPUT_PIN_WIDTH("RD_ADDR", 2)
    INPUT_PIN_WIDTH("WRITE_DATA", 32)
    INPUT_PIN("REG_WRITE")
    INPUT_PIN("CLK")
    INPUT_PIN("RST")
    OUTPUT_PIN_WIDTH("RS1_DATA", 32)
    OUTPUT_PIN_WIDTH("RS2_DATA", 32)
END_PINS()

void RegisterFile4x32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<ConstantValue<32, 32>>("X0_ZERO", 0);
    builder.addNewComponent<BehavioralRegister32>("X1");
    builder.addNewComponent<BehavioralRegister32>("X2");
    builder.addNewComponent<BehavioralRegister32>("X3");
    builder.addNewComponent<Mux4to1_32bit>("RS1_MUX");
    builder.addNewComponent<Mux4to1_32bit>("RS2_MUX");
    builder.addNewComponent<Decoder2to4>("RD_DECODER");

    builder.addNewWire<32>(
        "WRITE_DATA_internal",
        getInputPin<32>("WRITE_DATA"),
        {builder.getInputPin<ConstantValue<32, 32>, 32>("X0_ZERO", "TRIGGER"),
         builder.getInputPin<BehavioralRegister32, 32>("X1", "D"),
         builder.getInputPin<BehavioralRegister32, 32>("X2", "D"),
         builder.getInputPin<BehavioralRegister32, 32>("X3", "D")});

    builder.addNewWire<2>(
        "RD_ADDR_internal",
        getInputPin<2>("RD_ADDR"),
        {builder.getInputPin<Decoder2to4, 2>("RD_DECODER", "ADDR")});

    builder.addNewWire(
        "REG_WRITE_internal",
        getInputPin("REG_WRITE"),
        {builder.getInputPin<Decoder2to4>("RD_DECODER", "ENABLE")});

    builder.addNewWire(
        "X1_WE_internal",
        builder.getOutputPin<Decoder2to4>("RD_DECODER", "OUT1"),
        {builder.getInputPin<BehavioralRegister32>("X1", "WE")});

    builder.addNewWire(
        "X2_WE_internal",
        builder.getOutputPin<Decoder2to4>("RD_DECODER", "OUT2"),
        {builder.getInputPin<BehavioralRegister32>("X2", "WE")});

    builder.addNewWire(
        "X3_WE_internal",
        builder.getOutputPin<Decoder2to4>("RD_DECODER", "OUT3"),
        {builder.getInputPin<BehavioralRegister32>("X3", "WE")});

    builder.addNewWire(
        "CLK_internal",
        getInputPin("CLK"),
        {builder.getInputPin<BehavioralRegister32>("X1", "CLK"),
         builder.getInputPin<BehavioralRegister32>("X2", "CLK"),
         builder.getInputPin<BehavioralRegister32>("X3", "CLK")});

    builder.addNewWire(
        "RST_internal",
        getInputPin("RST"),
        {builder.getInputPin<BehavioralRegister32>("X1", "RST"),
         builder.getInputPin<BehavioralRegister32>("X2", "RST"),
         builder.getInputPin<BehavioralRegister32>("X3", "RST")});

    builder.addNewWire<2>(
        "RS1_ADDR_internal",
        getInputPin<2>("RS1_ADDR"),
        {builder.getInputPin<Mux4to1_32bit, 2>("RS1_MUX", "SEL")});

    builder.addNewWire<2>(
        "RS2_ADDR_internal",
        getInputPin<2>("RS2_ADDR"),
        {builder.getInputPin<Mux4to1_32bit, 2>("RS2_MUX", "SEL")});

    builder.addNewWire<32>(
        "X0_to_read_muxes",
        builder.getOutputPin<ConstantValue<32, 32>, 32>("X0_ZERO", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("RS1_MUX", "IN0"),
         builder.getInputPin<Mux4to1_32bit, 32>("RS2_MUX", "IN0")});

    for (size_t index = 1; index < 4; ++index) {
        const auto reg_name = "X" + std::to_string(index);
        const auto input_name = "IN" + std::to_string(index);
        builder.addNewWire<32>(
            reg_name + "_to_read_muxes",
            builder.getOutputPin<BehavioralRegister32, 32>(reg_name, "Q"),
            {builder.getInputPin<Mux4to1_32bit, 32>("RS1_MUX", input_name),
             builder.getInputPin<Mux4to1_32bit, 32>("RS2_MUX", input_name)});
    }

    builder.addNewWire<32>(
        "RS1_DATA_internal",
        builder.getOutputPin<Mux4to1_32bit, 32>("RS1_MUX", "OUT"),
        {getOutputPin<32>("RS1_DATA")});

    builder.addNewWire<32>(
        "RS2_DATA_internal",
        builder.getOutputPin<Mux4to1_32bit, 32>("RS2_MUX", "OUT"),
        {getOutputPin<32>("RS2_DATA")});
}
