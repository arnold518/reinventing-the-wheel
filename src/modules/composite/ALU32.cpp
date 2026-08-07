#include "modules/composite/ALU32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/ALU32Direct.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <memory>
#include <string>
#include <vector>

namespace {
void defineALU32Pins(IOComponent* self) {
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
}
} // namespace

namespace circuit::families {
const ComponentFamily ALU32{
    "rv32i.alu32",
    "ALU32",
    defineALU32Pins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::ALU32>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::ALU32Direct>(context, name);
    }};
}

ALU32::ALU32(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::ALU32.pinInitializer()) {}

void ALU32::buildInternals(ComponentBuilder& builder) {
    // One arithmetic carry chain serves ADD, SUB, SLT, SLTU, and the branch
    // comparison flags. The operation decoder selects subtraction only when
    // those comparison results are meaningful.
    builder.add(circuit::families::AddSub32, "ARITHMETIC");
    builder.add(circuit::families::Logic32, "LOGIC");
    builder.add(circuit::families::Shifter32, "SHIFT");
    builder.add(circuit::families::ZeroDetect32, "ARITHMETIC_ZERO");
    builder.add(circuit::families::ZeroDetect32, "RESULT_ZERO");
    builder.addNewComponent<BitSplitter<5>>("OP_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("ARITHMETIC_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("RESULT_SPLIT");
    builder.addNewComponent<BitJoiner<3>>("OP_LOW_JOIN");
    builder.addNewComponent<BitJoiner<32>>("SLT_JOIN");
    builder.addNewComponent<BitJoiner<32>>("SLTU_JOIN");
    builder.addNewComponent<ConstantValue<1>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<32>>("CONST_ZERO32", 0);

    // Two 8-way banks plus range selection replace the old 32-way result
    // selector. OP[4] clamps the architecturally unused half to zero.
    builder.addNewComponent<Mux8to1_32bit>("RESULT_LOW_MUX");
    builder.addNewComponent<Mux8to1_32bit>("RESULT_HIGH_MUX");
    builder.addNewComponent<Mux2to1_32bit>("RESULT_HALF_MUX");
    builder.addNewComponent<Mux2to1_32bit>("RESULT_RANGE_MUX");

    builder.addNewComponent<NOTGate>("NOT_OP4");
    builder.addNewComponent<NOTGate>("NOT_OP2");
    builder.addNewComponent<NOTGate>("NOT_OP1");
    builder.addNewComponent<ORGate>("SUB_SELECTOR_OR");
    builder.addNewComponent<ANDGate>("ARITHMETIC_VALID_1");
    builder.addNewComponent<ANDGate>("ARITHMETIC_VALID_2");
    builder.addNewComponent<ANDGate>("SUB_CONTROL");
    builder.addNewComponent<NOTGate>("NOT_CARRY");
    builder.addNewComponent<XORGate>("SIGNED_LT_RAW");
    builder.addNewComponent<ANDGate>("EQ_VALID");
    builder.addNewComponent<ANDGate>("SIGNED_LT_VALID");
    builder.addNewComponent<ANDGate>("UNSIGNED_LT_VALID");
    builder.addNewComponent<ANDGate>("CARRY_VALID");
    builder.addNewComponent<ANDGate>("OVERFLOW_VALID");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<AddSub32, 32>("ARITHMETIC", "A"),
         builder.getInputPin<Logic32, 32>("LOGIC", "A"),
         builder.getInputPin<Shifter32, 32>("SHIFT", "A"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN2")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<AddSub32, 32>("ARITHMETIC", "B"),
         builder.getInputPin<Logic32, 32>("LOGIC", "B"),
         builder.getInputPin<Shifter32, 32>("SHIFT", "B"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN3")});
    builder.addNewWire<5>(
        "OP_bus_internal",
        getInputPin<5>("OP"),
        {builder.getInputPin<BitSplitter<5>, 5>("OP_SPLIT", "IN")});

    // OP values 0,1,8,9 are the four arithmetic operations. Values 1,8,9
    // use subtraction. This small decoder is shared by result and flags.
    builder.addNewWire(
        "OP0_fanout",
        builder.getOutputPin<BitSplitter<5>>("OP_SPLIT", "OUT_0"),
        {builder.getInputPin<BitJoiner<3>>("OP_LOW_JOIN", "IN_0"),
         builder.getInputPin<ORGate>("SUB_SELECTOR_OR", "B"),
         builder.getInputPin<Shifter32>("SHIFT", "ARITHMETIC")});
    builder.addNewWire(
        "OP1_fanout",
        builder.getOutputPin<BitSplitter<5>>("OP_SPLIT", "OUT_1"),
        {builder.getInputPin<BitJoiner<3>>("OP_LOW_JOIN", "IN_1"),
         builder.getInputPin<NOTGate>("NOT_OP1", "IN")});
    builder.addNewWire(
        "OP2_fanout",
        builder.getOutputPin<BitSplitter<5>>("OP_SPLIT", "OUT_2"),
        {builder.getInputPin<BitJoiner<3>>("OP_LOW_JOIN", "IN_2"),
         builder.getInputPin<NOTGate>("NOT_OP2", "IN")});
    builder.addNewWire(
        "OP3_fanout",
        builder.getOutputPin<BitSplitter<5>>("OP_SPLIT", "OUT_3"),
        {builder.getInputPin<ORGate>("SUB_SELECTOR_OR", "A"),
         builder.getInputPin<Mux2to1_32bit>("RESULT_HALF_MUX", "SEL")});
    builder.addNewWire(
        "OP4_fanout",
        builder.getOutputPin<BitSplitter<5>>("OP_SPLIT", "OUT_4"),
        {builder.getInputPin<NOTGate>("NOT_OP4", "IN"),
         builder.getInputPin<Mux2to1_32bit>("RESULT_RANGE_MUX", "SEL")});
    builder.addNewWire(
        "NOT_OP4_to_arithmetic_valid",
        builder.getOutputPin<NOTGate>("NOT_OP4", "OUT"),
        {builder.getInputPin<ANDGate>("ARITHMETIC_VALID_1", "A")});
    builder.addNewWire(
        "NOT_OP2_to_arithmetic_valid",
        builder.getOutputPin<NOTGate>("NOT_OP2", "OUT"),
        {builder.getInputPin<ANDGate>("ARITHMETIC_VALID_1", "B")});
    builder.addNewWire(
        "ARITHMETIC_VALID_1_to_2",
        builder.getOutputPin<ANDGate>("ARITHMETIC_VALID_1", "OUT"),
        {builder.getInputPin<ANDGate>("ARITHMETIC_VALID_2", "A")});
    builder.addNewWire(
        "NOT_OP1_fanout",
        builder.getOutputPin<NOTGate>("NOT_OP1", "OUT"),
        {builder.getInputPin<ANDGate>("ARITHMETIC_VALID_2", "B"),
         builder.getInputPin<Shifter32>("SHIFT", "LEFT")});
    builder.addNewWire(
        "SUB_SELECTOR_to_control",
        builder.getOutputPin<ORGate>("SUB_SELECTOR_OR", "OUT"),
        {builder.getInputPin<ANDGate>("SUB_CONTROL", "B")});
    builder.addNewWire(
        "ARITHMETIC_VALID_fanout",
        builder.getOutputPin<ANDGate>("ARITHMETIC_VALID_2", "OUT"),
        {builder.getInputPin<ANDGate>("SUB_CONTROL", "A"),
         builder.getInputPin<ANDGate>("CARRY_VALID", "B"),
         builder.getInputPin<ANDGate>("OVERFLOW_VALID", "B")});
    builder.addNewWire(
        "SUB_CONTROL_fanout",
        builder.getOutputPin<ANDGate>("SUB_CONTROL", "OUT"),
        {builder.getInputPin<AddSub32>("ARITHMETIC", "SUB"),
         builder.getInputPin<ANDGate>("EQ_VALID", "B"),
         builder.getInputPin<ANDGate>("SIGNED_LT_VALID", "B"),
         builder.getInputPin<ANDGate>("UNSIGNED_LT_VALID", "B")});

    builder.addNewWire<3>(
        "OP_LOW_selector",
        builder.getOutputPin<BitJoiner<3>, 3>("OP_LOW_JOIN", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 3>("RESULT_LOW_MUX", "SEL"),
         builder.getInputPin<Mux8to1_32bit, 3>("RESULT_HIGH_MUX", "SEL")});

    builder.addNewWire<32>(
        "ARITHMETIC_result_fanout",
        builder.getOutputPin<AddSub32, 32>("ARITHMETIC", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN0"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN1"),
         builder.getInputPin<ZeroDetect32, 32>("ARITHMETIC_ZERO", "A"),
         builder.getInputPin<BitSplitter<32>, 32>("ARITHMETIC_SPLIT", "IN")});
    builder.addNewWire<32>(
        "AND_result_to_mux",
        builder.getOutputPin<Logic32, 32>("LOGIC", "AND_OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN2")});
    builder.addNewWire<32>(
        "OR_result_to_mux",
        builder.getOutputPin<Logic32, 32>("LOGIC", "OR_OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN3")});
    builder.addNewWire<32>(
        "XOR_result_to_mux",
        builder.getOutputPin<Logic32, 32>("LOGIC", "XOR_OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN4")});
    builder.addNewWire<32>(
        "SHIFT_result_to_mux",
        builder.getOutputPin<Shifter32, 32>("SHIFT", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN5"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN6"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "IN7")});

    builder.addNewWire(
        "ARITHMETIC_ZERO_to_eq",
        builder.getOutputPin<ZeroDetect32>("ARITHMETIC_ZERO", "ZERO"),
        {builder.getInputPin<ANDGate>("EQ_VALID", "A")});
    builder.addNewWire(
        "ARITHMETIC_sign_to_signed_lt",
        builder.getOutputPin<BitSplitter<32>>(
            "ARITHMETIC_SPLIT", "OUT_31"),
        {builder.getInputPin<XORGate>("SIGNED_LT_RAW", "A")});
    builder.addNewWire(
        "ARITHMETIC_carry_fanout",
        builder.getOutputPin<AddSub32>("ARITHMETIC", "CARRY_OUT"),
        {builder.getInputPin<NOTGate>("NOT_CARRY", "IN"),
         builder.getInputPin<ANDGate>("CARRY_VALID", "A")});
    builder.addNewWire(
        "ARITHMETIC_overflow_fanout",
        builder.getOutputPin<AddSub32>("ARITHMETIC", "OVERFLOW"),
        {builder.getInputPin<XORGate>("SIGNED_LT_RAW", "B"),
         builder.getInputPin<ANDGate>("OVERFLOW_VALID", "A")});
    builder.addNewWire(
        "SIGNED_LT_RAW_to_valid",
        builder.getOutputPin<XORGate>("SIGNED_LT_RAW", "OUT"),
        {builder.getInputPin<ANDGate>("SIGNED_LT_VALID", "A")});
    builder.addNewWire(
        "UNSIGNED_LT_RAW_to_valid",
        builder.getOutputPin<NOTGate>("NOT_CARRY", "OUT"),
        {builder.getInputPin<ANDGate>("UNSIGNED_LT_VALID", "A")});

    builder.addNewWire(
        "EQ_compare_result",
        builder.getOutputPin<ANDGate>("EQ_VALID", "OUT"),
        {getOutputPin("EQ")});
    builder.addNewWire(
        "SIGNED_LT_compare_result",
        builder.getOutputPin<ANDGate>("SIGNED_LT_VALID", "OUT"),
        {getOutputPin("LT_SIGNED"),
         builder.getInputPin<BitJoiner<32>>("SLT_JOIN", "IN_0")});
    builder.addNewWire(
        "UNSIGNED_LT_compare_result",
        builder.getOutputPin<ANDGate>("UNSIGNED_LT_VALID", "OUT"),
        {getOutputPin("LT_UNSIGNED"),
         builder.getInputPin<BitJoiner<32>>("SLTU_JOIN", "IN_0")});
    builder.addNewWire(
        "CARRY_compare_result",
        builder.getOutputPin<ANDGate>("CARRY_VALID", "OUT"),
        {getOutputPin("CARRY_OUT")});
    builder.addNewWire(
        "OVERFLOW_compare_result",
        builder.getOutputPin<ANDGate>("OVERFLOW_VALID", "OUT"),
        {getOutputPin("OVERFLOW")});

    std::vector<std::shared_ptr<Pin<>>> low_sinks;
    low_sinks.reserve(62);
    for (size_t bit = 1; bit < 32; ++bit) {
        low_sinks.push_back(builder.getInputPin<BitJoiner<32>>(
            "SLT_JOIN", "IN_" + std::to_string(bit)));
        low_sinks.push_back(builder.getInputPin<BitJoiner<32>>(
            "SLTU_JOIN", "IN_" + std::to_string(bit)));
    }
    builder.addNewWire(
        "CONST_LOW_to_compare_words",
        builder.getOutputPin<ConstantValue<1>>("CONST_LOW", "OUT"),
        low_sinks);
    builder.addNewWire<32>(
        "SLT_word_to_mux",
        builder.getOutputPin<BitJoiner<32>, 32>("SLT_JOIN", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN0")});
    builder.addNewWire<32>(
        "SLTU_word_to_mux",
        builder.getOutputPin<BitJoiner<32>, 32>("SLTU_JOIN", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN1")});

    builder.addNewWire<32>(
        "CONST_ZERO32_fanout",
        builder.getOutputPin<ConstantValue<32>, 32>("CONST_ZERO32", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN4"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN5"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN6"),
         builder.getInputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "IN7"),
         builder.getInputPin<Mux2to1_32bit, 32>("RESULT_RANGE_MUX", "B")});
    builder.addNewWire<32>(
        "RESULT_LOW_to_half",
        builder.getOutputPin<Mux8to1_32bit, 32>("RESULT_LOW_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_32bit, 32>("RESULT_HALF_MUX", "A")});
    builder.addNewWire<32>(
        "RESULT_HIGH_to_half",
        builder.getOutputPin<Mux8to1_32bit, 32>("RESULT_HIGH_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_32bit, 32>("RESULT_HALF_MUX", "B")});
    builder.addNewWire<32>(
        "RESULT_HALF_to_range",
        builder.getOutputPin<Mux2to1_32bit, 32>("RESULT_HALF_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_32bit, 32>("RESULT_RANGE_MUX", "A")});
    builder.addNewWire<32>(
        "RESULT_bus_internal",
        builder.getOutputPin<Mux2to1_32bit, 32>("RESULT_RANGE_MUX", "OUT"),
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
}
