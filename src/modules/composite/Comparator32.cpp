#include "modules/composite/Comparator32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"

Comparator32::Comparator32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
          self->addPin<32>("A", PinType::INPUT);
          self->addPin<32>("B", PinType::INPUT);
          self->addPin("EQ", PinType::OUTPUT);
          self->addPin("LT_SIGNED", PinType::OUTPUT);
          self->addPin("LT_UNSIGNED", PinType::OUTPUT);
          self->addPin<32>("DIFF", PinType::OUTPUT);
          self->addPin("CARRY_OUT", PinType::OUTPUT);
          self->addPin("OVERFLOW", PinType::OUTPUT);
      }) {}

void Comparator32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<AddSub32>("SUB");
    builder.addNewComponent<ZeroDetect32>("ZERO_DETECT");
    builder.addNewComponent<BitSplitter<32>>("DIFF_SPLIT");
    builder.addNewComponent<ConstantValue<1, 32>>("CONST_HIGH", 1);
    builder.addNewComponent<NOTGate>("NOT_CARRY");
    builder.addNewComponent<XORGate>("SIGNED_LT_XOR");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<AddSub32, 32>("SUB", "A")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<AddSub32, 32>("SUB", "B")});
    builder.addNewWire(
        "CONST_HIGH_to_SUB",
        builder.getOutputPin<ConstantValue<1, 32>>("CONST_HIGH", "OUT"),
        {builder.getInputPin<AddSub32>("SUB", "SUB")});

    builder.addNewWire<32>(
        "diff_bus_fanout",
        builder.getOutputPin<AddSub32, 32>("SUB", "OUT"),
        {getOutputPin<32>("DIFF"),
         builder.getInputPin<ZeroDetect32, 32>("ZERO_DETECT", "A"),
         builder.getInputPin<BitSplitter<32>, 32>("DIFF_SPLIT", "IN")});
    builder.addNewWire(
        "zero_detect_to_EQ",
        builder.getOutputPin<ZeroDetect32>("ZERO_DETECT", "ZERO"),
        {getOutputPin("EQ")});
    builder.addNewWire(
        "carry_out_fanout",
        builder.getOutputPin<AddSub32>("SUB", "CARRY_OUT"),
        {getOutputPin("CARRY_OUT"),
         builder.getInputPin<NOTGate>("NOT_CARRY", "IN")});
    builder.addNewWire(
        "not_carry_to_LT_UNSIGNED",
        builder.getOutputPin<NOTGate>("NOT_CARRY", "OUT"),
        {getOutputPin("LT_UNSIGNED")});
    builder.addNewWire(
        "diff_sign_to_signed_lt",
        builder.getOutputPin<BitSplitter<32>>("DIFF_SPLIT", "OUT_31"),
        {builder.getInputPin<XORGate>("SIGNED_LT_XOR", "A")});
    builder.addNewWire(
        "overflow_fanout",
        builder.getOutputPin<AddSub32>("SUB", "OVERFLOW"),
        {getOutputPin("OVERFLOW"),
         builder.getInputPin<XORGate>("SIGNED_LT_XOR", "B")});
    builder.addNewWire(
        "signed_lt_to_output",
        builder.getOutputPin<XORGate>("SIGNED_LT_XOR", "OUT"),
        {getOutputPin("LT_SIGNED")});
}
