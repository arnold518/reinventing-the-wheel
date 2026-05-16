#include "modules/composite/ALU32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <memory>
#include <string>
#include <vector>

namespace {
void addLowFlagInputs(std::vector<std::shared_ptr<Pin<>>>& sinks, ComponentBuilder& builder, const std::string& mux_name) {
    for (size_t input = 0; input < 32; ++input) {
        if (input == ALU32Op::ADD || input == ALU32Op::SUB ||
            input == ALU32Op::SLT || input == ALU32Op::SLTU) {
            continue;
        }
        sinks.push_back(builder.getInputPin<Mux32to1>(mux_name, "IN" + std::to_string(input)));
    }
}
}

ALU32::ALU32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
          self->addPin<32>("A", PinType::INPUT);
          self->addPin<32>("B", PinType::INPUT);
          self->addPin<5>("OP", PinType::INPUT);
          self->addPin<32>("OUT", PinType::OUTPUT);
          self->addPin("ZERO", PinType::OUTPUT);
          self->addPin("EQ", PinType::OUTPUT);
          self->addPin("LT_SIGNED", PinType::OUTPUT);
          self->addPin("LT_UNSIGNED", PinType::OUTPUT);
          self->addPin("NEGATIVE", PinType::OUTPUT);
          self->addPin("CARRY_OUT", PinType::OUTPUT);
          self->addPin("OVERFLOW", PinType::OUTPUT);
      }) {}

void ALU32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<AddSub32>("ADD");
    builder.addNewComponent<AddSub32>("SUB");
    builder.addNewComponent<Logic32>("LOGIC");
    builder.addNewComponent<Shifter32>("SHIFT");
    builder.addNewComponent<Comparator32>("CMP");
    builder.addNewComponent<ZeroDetect32>("RESULT_ZERO");
    builder.addNewComponent<BitSplitter<32>>("RESULT_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("SLT_JOIN");
    builder.addNewComponent<BitJoiner<32>>("SLTU_JOIN");
    builder.addNewComponent<ConstantValue<1, 32>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<1, 32>>("CONST_HIGH", 1);
    builder.addNewComponent<ConstantValue<32, 32>>("CONST_ZERO32", 0);
    builder.addNewComponent<Mux32to1_32bit>("RESULT_MUX");
    builder.addNewComponent<Mux32to1>("CARRY_MUX");
    builder.addNewComponent<Mux32to1>("OVERFLOW_MUX");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<AddSub32, 32>("ADD", "A"),
         builder.getInputPin<AddSub32, 32>("SUB", "A"),
         builder.getInputPin<Logic32, 32>("LOGIC", "A"),
         builder.getInputPin<Shifter32, 32>("SHIFT", "A"),
         builder.getInputPin<Comparator32, 32>("CMP", "A"),
         builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::PASS_A)),
         builder.getInputPin<ConstantValue<1, 32>, 32>("CONST_LOW", "TRIGGER"),
         builder.getInputPin<ConstantValue<1, 32>, 32>("CONST_HIGH", "TRIGGER"),
         builder.getInputPin<ConstantValue<32, 32>, 32>("CONST_ZERO32", "TRIGGER")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<AddSub32, 32>("ADD", "B"),
         builder.getInputPin<AddSub32, 32>("SUB", "B"),
         builder.getInputPin<Logic32, 32>("LOGIC", "B"),
         builder.getInputPin<Shifter32, 32>("SHIFT", "B"),
         builder.getInputPin<Comparator32, 32>("CMP", "B"),
         builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::PASS_B))});
    builder.addNewWire<5>(
        "OP_bus_internal",
        getInputPin<5>("OP"),
        {builder.getInputPin<Mux32to1_32bit, 5>("RESULT_MUX", "SEL"),
         builder.getInputPin<Mux32to1, 5>("CARRY_MUX", "SEL"),
         builder.getInputPin<Mux32to1, 5>("OVERFLOW_MUX", "SEL")});

    std::vector<std::shared_ptr<Pin<>>> low_sinks{
        builder.getInputPin<AddSub32>("ADD", "SUB"),
    };
    for (size_t bit = 1; bit < 32; ++bit) {
        low_sinks.push_back(builder.getInputPin<BitJoiner<32>>("SLT_JOIN", "IN_" + std::to_string(bit)));
        low_sinks.push_back(builder.getInputPin<BitJoiner<32>>("SLTU_JOIN", "IN_" + std::to_string(bit)));
    }
    addLowFlagInputs(low_sinks, builder, "CARRY_MUX");
    addLowFlagInputs(low_sinks, builder, "OVERFLOW_MUX");
    builder.addNewWire(
        "CONST_LOW_fanout",
        builder.getOutputPin<ConstantValue<1, 32>>("CONST_LOW", "OUT"),
        low_sinks);
    builder.addNewWire(
        "CONST_HIGH_to_SUB",
        builder.getOutputPin<ConstantValue<1, 32>>("CONST_HIGH", "OUT"),
        {builder.getInputPin<AddSub32>("SUB", "SUB")});

    builder.addNewWire<32>(
        "ADD_result_to_mux",
        builder.getOutputPin<AddSub32, 32>("ADD", "OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::ADD))});
    builder.addNewWire<32>(
        "SUB_result_to_mux",
        builder.getOutputPin<AddSub32, 32>("SUB", "OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::SUB))});
    builder.addNewWire<32>(
        "AND_result_to_mux",
        builder.getOutputPin<Logic32, 32>("LOGIC", "AND_OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::AND))});
    builder.addNewWire<32>(
        "OR_result_to_mux",
        builder.getOutputPin<Logic32, 32>("LOGIC", "OR_OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::OR))});
    builder.addNewWire<32>(
        "XOR_result_to_mux",
        builder.getOutputPin<Logic32, 32>("LOGIC", "XOR_OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::XOR))});
    builder.addNewWire<32>(
        "SLL_result_to_mux",
        builder.getOutputPin<Shifter32, 32>("SHIFT", "SLL_OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::SLL))});
    builder.addNewWire<32>(
        "SRL_result_to_mux",
        builder.getOutputPin<Shifter32, 32>("SHIFT", "SRL_OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::SRL))});
    builder.addNewWire<32>(
        "SRA_result_to_mux",
        builder.getOutputPin<Shifter32, 32>("SHIFT", "SRA_OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::SRA))});

    builder.addNewWire(
        "CMP_EQ_to_output",
        builder.getOutputPin<Comparator32>("CMP", "EQ"),
        {getOutputPin("EQ")});
    builder.addNewWire(
        "CMP_LT_SIGNED_fanout",
        builder.getOutputPin<Comparator32>("CMP", "LT_SIGNED"),
        {getOutputPin("LT_SIGNED"),
         builder.getInputPin<BitJoiner<32>>("SLT_JOIN", "IN_0")});
    builder.addNewWire(
        "CMP_LT_UNSIGNED_fanout",
        builder.getOutputPin<Comparator32>("CMP", "LT_UNSIGNED"),
        {getOutputPin("LT_UNSIGNED"),
         builder.getInputPin<BitJoiner<32>>("SLTU_JOIN", "IN_0")});
    builder.addNewWire<32>(
        "SLT_bus_to_mux",
        builder.getOutputPin<BitJoiner<32>, 32>("SLT_JOIN", "OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::SLT))});
    builder.addNewWire<32>(
        "SLTU_bus_to_mux",
        builder.getOutputPin<BitJoiner<32>, 32>("SLTU_JOIN", "OUT"),
        {builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(ALU32Op::SLTU))});

    std::vector<std::shared_ptr<Pin<32>>> zero32_sinks;
    for (size_t input = ALU32Op::ZERO; input < 32; ++input) {
        zero32_sinks.push_back(builder.getInputPin<Mux32to1_32bit, 32>("RESULT_MUX", "IN" + std::to_string(input)));
    }
    builder.addNewWire<32>(
        "CONST_ZERO32_to_mux",
        builder.getOutputPin<ConstantValue<32, 32>, 32>("CONST_ZERO32", "OUT"),
        zero32_sinks);

    builder.addNewWire(
        "ADD_carry_to_mux",
        builder.getOutputPin<AddSub32>("ADD", "CARRY_OUT"),
        {builder.getInputPin<Mux32to1>("CARRY_MUX", "IN" + std::to_string(ALU32Op::ADD))});
    builder.addNewWire(
        "SUB_carry_to_mux",
        builder.getOutputPin<AddSub32>("SUB", "CARRY_OUT"),
        {builder.getInputPin<Mux32to1>("CARRY_MUX", "IN" + std::to_string(ALU32Op::SUB))});
    builder.addNewWire(
        "CMP_carry_to_mux",
        builder.getOutputPin<Comparator32>("CMP", "CARRY_OUT"),
        {builder.getInputPin<Mux32to1>("CARRY_MUX", "IN" + std::to_string(ALU32Op::SLT)),
         builder.getInputPin<Mux32to1>("CARRY_MUX", "IN" + std::to_string(ALU32Op::SLTU))});
    builder.addNewWire(
        "ADD_overflow_to_mux",
        builder.getOutputPin<AddSub32>("ADD", "OVERFLOW"),
        {builder.getInputPin<Mux32to1>("OVERFLOW_MUX", "IN" + std::to_string(ALU32Op::ADD))});
    builder.addNewWire(
        "SUB_overflow_to_mux",
        builder.getOutputPin<AddSub32>("SUB", "OVERFLOW"),
        {builder.getInputPin<Mux32to1>("OVERFLOW_MUX", "IN" + std::to_string(ALU32Op::SUB))});
    builder.addNewWire(
        "CMP_overflow_to_mux",
        builder.getOutputPin<Comparator32>("CMP", "OVERFLOW"),
        {builder.getInputPin<Mux32to1>("OVERFLOW_MUX", "IN" + std::to_string(ALU32Op::SLT)),
         builder.getInputPin<Mux32to1>("OVERFLOW_MUX", "IN" + std::to_string(ALU32Op::SLTU))});

    builder.addNewWire<32>(
        "RESULT_bus_internal",
        builder.getOutputPin<Mux32to1_32bit, 32>("RESULT_MUX", "OUT"),
        {getOutputPin<32>("OUT"),
         builder.getInputPin<ZeroDetect32, 32>("RESULT_ZERO", "A"),
         builder.getInputPin<BitSplitter<32>, 32>("RESULT_SPLIT", "IN")});
    builder.addNewWire(
        "RESULT_ZERO_to_ZERO",
        builder.getOutputPin<ZeroDetect32>("RESULT_ZERO", "ZERO"),
        {getOutputPin("ZERO")});
    builder.addNewWire(
        "RESULT_sign_to_NEGATIVE",
        builder.getOutputPin<BitSplitter<32>>("RESULT_SPLIT", "OUT_31"),
        {getOutputPin("NEGATIVE")});
    builder.addNewWire(
        "CARRY_MUX_to_CARRY_OUT",
        builder.getOutputPin<Mux32to1>("CARRY_MUX", "OUT"),
        {getOutputPin("CARRY_OUT")});
    builder.addNewWire(
        "OVERFLOW_MUX_to_OVERFLOW",
        builder.getOutputPin<Mux32to1>("OVERFLOW_MUX", "OUT"),
        {getOutputPin("OVERFLOW")});
}
