#include "modules/composite/Arithmetic8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Comparator8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <cstdint>

namespace {
void connectAdderResult(ComponentBuilder& builder, IOComponent& component, const std::string& adder_name,
                        const std::string& result_pin) {
    builder.addNewWire<8>(
        adder_name + "_sum_to_" + result_pin,
        builder.getOutputPin<Adder8, 8>(adder_name, "Sum"),
        {component.getOutputPin<8>(result_pin)});
}

void buildEqualityFlag(ComponentBuilder& builder, IOComponent& component, uint64_t value,
                       const std::string& eq_name, const std::string& constant_name,
                       const std::string& output_pin) {
    builder.addNewComponent<EqualityChecker8>(eq_name);
    builder.addNewComponent<ConstantValue<8, 8>>(constant_name, value);
    builder.addNewWire<8>(
        constant_name + "_to_" + eq_name,
        builder.getOutputPin<ConstantValue<8, 8>, 8>(constant_name, "OUT"),
        {builder.getInputPin<EqualityChecker8, 8>(eq_name, "B")});
    builder.addNewWire(
        eq_name + "_to_" + output_pin,
        builder.getOutputPin<EqualityChecker8>(eq_name, "EQ"),
        {component.getOutputPin(output_pin)});
}

void buildSubOverflow(ComponentBuilder& builder, IOComponent& component,
                      const std::string& result_source_name) {
    builder.addNewComponent<BitSplitter<8>>("SUB_OVERFLOW_A_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("SUB_OVERFLOW_B_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("SUB_OVERFLOW_RESULT_SPLIT");
    builder.addNewComponent<XORGate>("SUB_OVERFLOW_A_XOR_B");
    builder.addNewComponent<XORGate>("SUB_OVERFLOW_A_XOR_RESULT");
    builder.addNewComponent<ANDGate>("SUB_OVERFLOW_AND");

    builder.addNewWire<8>(
        result_source_name + "_to_Result",
        builder.getOutputPin<Adder8, 8>(result_source_name, "Sum"),
        {component.getOutputPin<8>("Result"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_RESULT_SPLIT", "IN")});
    builder.addNewWire(
        "SUB_OVERFLOW_to_Overflow",
        builder.getOutputPin<ANDGate>("SUB_OVERFLOW_AND", "OUT"),
        {component.getOutputPin("Overflow")});
    builder.addNewWire(
        "SUB_OVERFLOW_A7_to_XORs",
        builder.getOutputPin<BitSplitter<8>>("SUB_OVERFLOW_A_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_B", "A"),
         builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_RESULT", "A")});
    builder.addNewWire(
        "SUB_OVERFLOW_B7_to_A_XOR_B",
        builder.getOutputPin<BitSplitter<8>>("SUB_OVERFLOW_B_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_B", "B")});
    builder.addNewWire(
        "SUB_OVERFLOW_RESULT7_to_A_XOR_RESULT",
        builder.getOutputPin<BitSplitter<8>>("SUB_OVERFLOW_RESULT_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_RESULT", "B")});
    builder.addNewWire(
        "SUB_OVERFLOW_A_XOR_B_to_AND",
        builder.getOutputPin<XORGate>("SUB_OVERFLOW_A_XOR_B", "OUT"),
        {builder.getInputPin<ANDGate>("SUB_OVERFLOW_AND", "A")});
    builder.addNewWire(
        "SUB_OVERFLOW_A_XOR_RESULT_to_AND",
        builder.getOutputPin<XORGate>("SUB_OVERFLOW_A_XOR_RESULT", "OUT"),
        {builder.getInputPin<ANDGate>("SUB_OVERFLOW_AND", "B")});
}
}

TwosComplement8::TwosComplement8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void TwosComplement8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<NOT8>("NOT_A");
    builder.addNewComponent<Adder8>("ADD_ONE");
    builder.addNewComponent<ZeroDetect8>("ZERO_DETECT");
    builder.addNewComponent<NOTGate>("NOT_ZERO");
    builder.addNewComponent<ConstantValue<8, 8>>("CONST_ONE", 0x01);
    builder.addNewComponent<ConstantValue<1, 8>>("CONST_LOW", 0);
    buildEqualityFlag(builder, *this, 0x80, "EQ_80", "CONST_80", "Overflow");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<NOT8, 8>("NOT_A", "A"),
         builder.getInputPin<ZeroDetect8, 8>("ZERO_DETECT", "A"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_80", "A")});
    builder.addNewWire<8>(
        "NOT_A_to_ADD",
        builder.getOutputPin<NOT8, 8>("NOT_A", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD_ONE", "A")});
    builder.addNewWire<8>(
        "CONST_ONE_to_ADD",
        builder.getOutputPin<ConstantValue<8, 8>, 8>("CONST_ONE", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD_ONE", "B")});
    builder.addNewWire(
        "CONST_LOW_to_Cin",
        builder.getOutputPin<ConstantValue<1, 8>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Adder8>("ADD_ONE", "Cin")});
    connectAdderResult(builder, *this, "ADD_ONE", "Result");
    builder.addNewWire(
        "ZERO_to_NOT",
        builder.getOutputPin<ZeroDetect8>("ZERO_DETECT", "ZERO"),
        {builder.getInputPin<NOTGate>("NOT_ZERO", "IN")});
    builder.addNewWire(
        "NOT_ZERO_to_Cout",
        builder.getOutputPin<NOTGate>("NOT_ZERO", "OUT"),
        {getOutputPin("Cout")});
}

Subtractor8::Subtractor8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Subtractor8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<NOT8>("NOT_B");
    builder.addNewComponent<Adder8>("ADD");
    builder.addNewComponent<ConstantValue<1, 8>>("CONST_HIGH", 1);
    buildSubOverflow(builder, *this, "ADD");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_A_SPLIT", "IN")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<NOT8, 8>("NOT_B", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_B_SPLIT", "IN")});
    builder.addNewWire<8>(
        "NOT_B_to_ADD",
        builder.getOutputPin<NOT8, 8>("NOT_B", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    builder.addNewWire(
        "CONST_HIGH_to_Cin",
        builder.getOutputPin<ConstantValue<1, 8>>("CONST_HIGH", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    builder.addNewWire(
        "ADD_Cout_to_Cout",
        builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Cout")});
}

SubtractorWithBorrow8::SubtractorWithBorrow8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("Bin", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Bout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void SubtractorWithBorrow8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<NOT8>("NOT_B");
    builder.addNewComponent<NOTGate>("NOT_BORROW");
    builder.addNewComponent<Adder8>("ADD");
    buildSubOverflow(builder, *this, "ADD");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_A_SPLIT", "IN")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<NOT8, 8>("NOT_B", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_B_SPLIT", "IN")});
    builder.addNewWire(
        "Bin_to_NOT",
        getInputPin("Bin"),
        {builder.getInputPin<NOTGate>("NOT_BORROW", "IN")});
    builder.addNewWire<8>(
        "NOT_B_to_ADD",
        builder.getOutputPin<NOT8, 8>("NOT_B", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    builder.addNewWire(
        "NOT_BORROW_to_Cin",
        builder.getOutputPin<NOTGate>("NOT_BORROW", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    builder.addNewWire(
        "ADD_Cout_to_Bout",
        builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Bout")});
}

Incrementer8::Incrementer8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Incrementer8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Adder8>("ADD");
    builder.addNewComponent<ConstantValue<8, 8>>("CONST_ZERO", 0x00);
    builder.addNewComponent<ConstantValue<1, 8>>("CONST_HIGH", 1);
    buildEqualityFlag(builder, *this, 0x7F, "EQ_7F", "CONST_7F", "Overflow");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_7F", "A")});
    builder.addNewWire<8>(
        "CONST_ZERO_to_ADD",
        builder.getOutputPin<ConstantValue<8, 8>, 8>("CONST_ZERO", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    builder.addNewWire(
        "CONST_HIGH_to_Cin",
        builder.getOutputPin<ConstantValue<1, 8>>("CONST_HIGH", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    connectAdderResult(builder, *this, "ADD", "Result");
    builder.addNewWire(
        "ADD_Cout_to_Cout",
        builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Cout")});
}

Decrementer8::Decrementer8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Bout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Decrementer8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Adder8>("ADD");
    builder.addNewComponent<ConstantValue<8, 8>>("CONST_FF", 0xFF);
    builder.addNewComponent<ConstantValue<1, 8>>("CONST_LOW", 0);
    buildEqualityFlag(builder, *this, 0x80, "EQ_80", "CONST_80", "Overflow");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_80", "A")});
    builder.addNewWire<8>(
        "CONST_FF_to_ADD",
        builder.getOutputPin<ConstantValue<8, 8>, 8>("CONST_FF", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    builder.addNewWire(
        "CONST_LOW_to_Cin",
        builder.getOutputPin<ConstantValue<1, 8>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    connectAdderResult(builder, *this, "ADD", "Result");
    builder.addNewWire(
        "ADD_Cout_to_Bout",
        builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Bout")});
}
