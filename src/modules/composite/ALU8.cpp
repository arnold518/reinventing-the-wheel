#include "modules/composite/ALU8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/Shifter8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
void addLowSink(std::vector<std::shared_ptr<Pin<>>>& sinks, ComponentBuilder& builder,
                const std::string& mux_name, size_t input_index) {
    sinks.push_back(builder.getInputPin<Mux16to1>(mux_name, "IN" + std::to_string(input_index)));
}
}

ALU8::ALU8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<4>("OP", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
        self->addPin("ZERO", PinType::OUTPUT);
        self->addPin("CARRY", PinType::OUTPUT);
        self->addPin("OVERFLOW", PinType::OUTPUT);
        self->addPin("NEGATIVE", PinType::OUTPUT);
    }) {}

void ALU8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Adder8>("ADD");
    builder.addNewComponent<Subtractor8>("SUB");
    builder.addNewComponent<AND8>("AND");
    builder.addNewComponent<OR8>("OR");
    builder.addNewComponent<XOR8>("XOR");
    builder.addNewComponent<NOT8>("NOT_A");
    builder.addNewComponent<ShiftLeftLogical8>("SLL");
    builder.addNewComponent<ShiftRightLogical8>("SRL");
    builder.addNewComponent<ShiftRightArithmetic8>("SRA");
    builder.addNewComponent<Incrementer8>("INC");
    builder.addNewComponent<Decrementer8>("DEC");
    builder.addNewComponent<TwosComplement8>("TWOS");
    builder.addNewComponent<BitSplitter<8>>("ADD_OVERFLOW_A_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("ADD_OVERFLOW_B_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("ADD_OVERFLOW_RESULT_SPLIT");
    builder.addNewComponent<XORGate>("ADD_OVERFLOW_A_XOR_B");
    builder.addNewComponent<NOTGate>("ADD_OVERFLOW_SAME_SIGN");
    builder.addNewComponent<XORGate>("ADD_OVERFLOW_A_XOR_RESULT");
    builder.addNewComponent<ANDGate>("ADD_OVERFLOW_AND");

    builder.addNewComponent<Mux16to1_8bit>("RESULT_MUX");
    builder.addNewComponent<Mux16to1>("CARRY_MUX");
    builder.addNewComponent<Mux16to1>("OVERFLOW_MUX");
    builder.addNewComponent<ZeroDetect8>("ZERO_DETECT");
    builder.addNewComponent<BitSplitter<8>>("RESULT_SPLIT");
    builder.addNewComponent<ConstantValue<1, 8>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<8, 8>>("CONST_ZERO8", 0x00);

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<Subtractor8, 8>("SUB", "A"),
         builder.getInputPin<AND8, 8>("AND", "A"),
         builder.getInputPin<OR8, 8>("OR", "A"),
         builder.getInputPin<XOR8, 8>("XOR", "A"),
         builder.getInputPin<NOT8, 8>("NOT_A", "A"),
         builder.getInputPin<ShiftLeftLogical8, 8>("SLL", "A"),
         builder.getInputPin<ShiftRightLogical8, 8>("SRL", "A"),
         builder.getInputPin<ShiftRightArithmetic8, 8>("SRA", "A"),
         builder.getInputPin<Incrementer8, 8>("INC", "A"),
         builder.getInputPin<Decrementer8, 8>("DEC", "A"),
         builder.getInputPin<TwosComplement8, 8>("TWOS", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("ADD_OVERFLOW_A_SPLIT", "IN"),
         builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN12")});

    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<Adder8, 8>("ADD", "B"),
         builder.getInputPin<Subtractor8, 8>("SUB", "B"),
         builder.getInputPin<AND8, 8>("AND", "B"),
         builder.getInputPin<OR8, 8>("OR", "B"),
         builder.getInputPin<XOR8, 8>("XOR", "B"),
         builder.getInputPin<BitSplitter<8>, 8>("ADD_OVERFLOW_B_SPLIT", "IN"),
         builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN13")});

    builder.addNewWire<4>(
        "OP_bus_internal",
        getInputPin<4>("OP"),
        {builder.getInputPin<Mux16to1_8bit, 4>("RESULT_MUX", "SEL"),
         builder.getInputPin<Mux16to1, 4>("CARRY_MUX", "SEL"),
         builder.getInputPin<Mux16to1, 4>("OVERFLOW_MUX", "SEL")});

    std::vector<std::shared_ptr<Pin<>>> low_sinks{
        builder.getInputPin<Adder8>("ADD", "Cin"),
    };
    for (const auto input_index : {2U, 3U, 4U, 5U, 12U, 13U, 15U}) {
        addLowSink(low_sinks, builder, "CARRY_MUX", input_index);
    }
    for (const auto input_index : {2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 13U, 15U}) {
        addLowSink(low_sinks, builder, "OVERFLOW_MUX", input_index);
    }
    builder.addNewWire(
        "CONST_LOW_to_flags",
        builder.getOutputPin<ConstantValue<1, 8>>("CONST_LOW", "OUT"),
        low_sinks);

    builder.addNewWire<8>(
        "ADD_result_fanout",
        builder.getOutputPin<Adder8, 8>("ADD", "Sum"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN0"),
         builder.getInputPin<BitSplitter<8>, 8>("ADD_OVERFLOW_RESULT_SPLIT", "IN")});
    builder.addNewWire(
        "ADD_Cout_to_CARRY_MUX",
        builder.getOutputPin<Adder8>("ADD", "Cout"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN0")});
    builder.addNewWire(
        "ADD_Overflow_to_OVERFLOW_MUX",
        builder.getOutputPin<ANDGate>("ADD_OVERFLOW_AND", "OUT"),
        {builder.getInputPin<Mux16to1>("OVERFLOW_MUX", "IN0")});
    builder.addNewWire(
        "ADD_overflow_A7_to_A_XOR_B",
        builder.getOutputPin<BitSplitter<8>>("ADD_OVERFLOW_A_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("ADD_OVERFLOW_A_XOR_B", "A"),
         builder.getInputPin<XORGate>("ADD_OVERFLOW_A_XOR_RESULT", "A")});
    builder.addNewWire(
        "ADD_overflow_B7_to_A_XOR_B",
        builder.getOutputPin<BitSplitter<8>>("ADD_OVERFLOW_B_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("ADD_OVERFLOW_A_XOR_B", "B")});
    builder.addNewWire(
        "ADD_overflow_result7_to_A_XOR_RESULT",
        builder.getOutputPin<BitSplitter<8>>("ADD_OVERFLOW_RESULT_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("ADD_OVERFLOW_A_XOR_RESULT", "B")});
    builder.addNewWire(
        "ADD_overflow_A_XOR_B_to_NOT",
        builder.getOutputPin<XORGate>("ADD_OVERFLOW_A_XOR_B", "OUT"),
        {builder.getInputPin<NOTGate>("ADD_OVERFLOW_SAME_SIGN", "IN")});
    builder.addNewWire(
        "ADD_overflow_same_sign_to_AND",
        builder.getOutputPin<NOTGate>("ADD_OVERFLOW_SAME_SIGN", "OUT"),
        {builder.getInputPin<ANDGate>("ADD_OVERFLOW_AND", "A")});
    builder.addNewWire(
        "ADD_overflow_A_XOR_RESULT_to_AND",
        builder.getOutputPin<XORGate>("ADD_OVERFLOW_A_XOR_RESULT", "OUT"),
        {builder.getInputPin<ANDGate>("ADD_OVERFLOW_AND", "B")});

    builder.addNewWire<8>(
        "SUB_result_fanout",
        builder.getOutputPin<Subtractor8, 8>("SUB", "Result"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN1"),
         builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN14")});
    builder.addNewWire(
        "SUB_Cout_fanout",
        builder.getOutputPin<Subtractor8>("SUB", "Cout"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN1"),
         builder.getInputPin<Mux16to1>("CARRY_MUX", "IN14")});
    builder.addNewWire(
        "SUB_Overflow_fanout",
        builder.getOutputPin<Subtractor8>("SUB", "Overflow"),
        {builder.getInputPin<Mux16to1>("OVERFLOW_MUX", "IN1"),
         builder.getInputPin<Mux16to1>("OVERFLOW_MUX", "IN14")});

    builder.addNewWire<8>(
        "AND_to_RESULT_MUX",
        builder.getOutputPin<AND8, 8>("AND", "OUT"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN2")});
    builder.addNewWire<8>(
        "OR_to_RESULT_MUX",
        builder.getOutputPin<OR8, 8>("OR", "OUT"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN3")});
    builder.addNewWire<8>(
        "XOR_to_RESULT_MUX",
        builder.getOutputPin<XOR8, 8>("XOR", "OUT"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN4")});
    builder.addNewWire<8>(
        "NOT_to_RESULT_MUX",
        builder.getOutputPin<NOT8, 8>("NOT_A", "OUT"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN5")});

    builder.addNewWire<8>(
        "SLL_result_to_RESULT_MUX",
        builder.getOutputPin<ShiftLeftLogical8, 8>("SLL", "Result"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN6")});
    builder.addNewWire(
        "SLL_Carry_to_CARRY_MUX",
        builder.getOutputPin<ShiftLeftLogical8>("SLL", "Carry"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN6")});
    builder.addNewWire<8>(
        "SRL_result_to_RESULT_MUX",
        builder.getOutputPin<ShiftRightLogical8, 8>("SRL", "Result"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN7")});
    builder.addNewWire(
        "SRL_Carry_to_CARRY_MUX",
        builder.getOutputPin<ShiftRightLogical8>("SRL", "Carry"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN7")});
    builder.addNewWire<8>(
        "SRA_result_to_RESULT_MUX",
        builder.getOutputPin<ShiftRightArithmetic8, 8>("SRA", "Result"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN8")});
    builder.addNewWire(
        "SRA_Carry_to_CARRY_MUX",
        builder.getOutputPin<ShiftRightArithmetic8>("SRA", "Carry"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN8")});

    builder.addNewWire<8>(
        "INC_result_to_RESULT_MUX",
        builder.getOutputPin<Incrementer8, 8>("INC", "Result"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN9")});
    builder.addNewWire(
        "INC_Cout_to_CARRY_MUX",
        builder.getOutputPin<Incrementer8>("INC", "Cout"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN9")});
    builder.addNewWire(
        "INC_Overflow_to_OVERFLOW_MUX",
        builder.getOutputPin<Incrementer8>("INC", "Overflow"),
        {builder.getInputPin<Mux16to1>("OVERFLOW_MUX", "IN9")});

    builder.addNewWire<8>(
        "DEC_result_to_RESULT_MUX",
        builder.getOutputPin<Decrementer8, 8>("DEC", "Result"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN10")});
    builder.addNewWire(
        "DEC_Bout_to_CARRY_MUX",
        builder.getOutputPin<Decrementer8>("DEC", "Bout"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN10")});
    builder.addNewWire(
        "DEC_Overflow_to_OVERFLOW_MUX",
        builder.getOutputPin<Decrementer8>("DEC", "Overflow"),
        {builder.getInputPin<Mux16to1>("OVERFLOW_MUX", "IN10")});

    builder.addNewWire<8>(
        "TWOS_result_to_RESULT_MUX",
        builder.getOutputPin<TwosComplement8, 8>("TWOS", "Result"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN11")});
    builder.addNewWire(
        "TWOS_Cout_to_CARRY_MUX",
        builder.getOutputPin<TwosComplement8>("TWOS", "Cout"),
        {builder.getInputPin<Mux16to1>("CARRY_MUX", "IN11")});
    builder.addNewWire(
        "TWOS_Overflow_to_OVERFLOW_MUX",
        builder.getOutputPin<TwosComplement8>("TWOS", "Overflow"),
        {builder.getInputPin<Mux16to1>("OVERFLOW_MUX", "IN11")});

    builder.addNewWire<8>(
        "CONST_ZERO8_to_RESULT_MUX",
        builder.getOutputPin<ConstantValue<8, 8>, 8>("CONST_ZERO8", "OUT"),
        {builder.getInputPin<Mux16to1_8bit, 8>("RESULT_MUX", "IN15")});

    builder.addNewWire<8>(
        "RESULT_bus_internal",
        builder.getOutputPin<Mux16to1_8bit, 8>("RESULT_MUX", "OUT"),
        {getOutputPin<8>("OUT"),
         builder.getInputPin<ZeroDetect8, 8>("ZERO_DETECT", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("RESULT_SPLIT", "IN")});
    builder.addNewWire(
        "ZERO_DETECT_to_ZERO",
        builder.getOutputPin<ZeroDetect8>("ZERO_DETECT", "ZERO"),
        {getOutputPin("ZERO")});
    builder.addNewWire(
        "RESULT_sign_to_NEGATIVE",
        builder.getOutputPin<BitSplitter<8>>("RESULT_SPLIT", "OUT_7"),
        {getOutputPin("NEGATIVE")});
    builder.addNewWire(
        "CARRY_MUX_to_CARRY",
        builder.getOutputPin<Mux16to1>("CARRY_MUX", "OUT"),
        {getOutputPin("CARRY")});
    builder.addNewWire(
        "OVERFLOW_MUX_to_OVERFLOW",
        builder.getOutputPin<Mux16to1>("OVERFLOW_MUX", "OUT"),
        {getOutputPin("OVERFLOW")});
}
