#include "components/StructuralNetBuilder.hpp"
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
    circuit::detail::StructuralWiring w(builder);
    w.connect(builder.getOutputPin<Adder8, 8>(adder_name, "Sum"),
        {component.getOutputPin<8>(result_pin)});
    w.finish();
}

void buildEqualityFlag(ComponentBuilder& builder, IOComponent& component, uint64_t value,
                       const std::string& eq_name, const std::string& constant_name,
                       const std::string& output_pin) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<EqualityChecker8>(eq_name);
    builder.addNewComponent<ConstantValue<8>>(constant_name, value);
    w.connect(builder.getOutputPin<ConstantValue<8>, 8>(constant_name, "OUT"),
        {builder.getInputPin<EqualityChecker8, 8>(eq_name, "B")});
    w.connect(builder.getOutputPin<EqualityChecker8>(eq_name, "EQ"),
        {component.getOutputPin(output_pin)});
    w.finish();
}

void buildSubOverflow(ComponentBuilder& builder, IOComponent& component,
                      const std::string& result_source_name) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<8>>("SUB_OVERFLOW_A_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("SUB_OVERFLOW_B_SPLIT");
    builder.addNewComponent<BitSplitter<8>>("SUB_OVERFLOW_RESULT_SPLIT");
    builder.addNewComponent<XORGate>("SUB_OVERFLOW_A_XOR_B");
    builder.addNewComponent<XORGate>("SUB_OVERFLOW_A_XOR_RESULT");
    builder.addNewComponent<ANDGate>("SUB_OVERFLOW_AND");

    w.connect(builder.getOutputPin<Adder8, 8>(result_source_name, "Sum"),
        {component.getOutputPin<8>("Result"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_RESULT_SPLIT", "IN")});
    w.connect(builder.getOutputPin<ANDGate>("SUB_OVERFLOW_AND", "OUT"),
        {component.getOutputPin("Overflow")});
    w.connect(builder.getOutputPin<BitSplitter<8>>("SUB_OVERFLOW_A_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_B", "A"),
         builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_RESULT", "A")});
    w.connect(builder.getOutputPin<BitSplitter<8>>("SUB_OVERFLOW_B_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_B", "B")});
    w.connect(builder.getOutputPin<BitSplitter<8>>("SUB_OVERFLOW_RESULT_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("SUB_OVERFLOW_A_XOR_RESULT", "B")});
    w.connect(builder.getOutputPin<XORGate>("SUB_OVERFLOW_A_XOR_B", "OUT"),
        {builder.getInputPin<ANDGate>("SUB_OVERFLOW_AND", "A")});
    w.connect(builder.getOutputPin<XORGate>("SUB_OVERFLOW_A_XOR_RESULT", "OUT"),
        {builder.getInputPin<ANDGate>("SUB_OVERFLOW_AND", "B")});
    w.finish();
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
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOT8>("NOT_A");
    builder.addNewComponent<Adder8>("ADD_ONE");
    builder.addNewComponent<ZeroDetect8>("ZERO_DETECT");
    builder.addNewComponent<NOTGate>("NOT_ZERO");
    builder.addNewComponent<ConstantValue<8>>("CONST_ONE", 0x01);
    builder.addNewComponent<ConstantValue<1>>("CONST_LOW", 0);
    buildEqualityFlag(builder, *this, 0x80, "EQ_80", "CONST_80", "Overflow");

    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<NOT8, 8>("NOT_A", "A"),
         builder.getInputPin<ZeroDetect8, 8>("ZERO_DETECT", "A"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_80", "A")});
    w.connect(builder.getOutputPin<NOT8, 8>("NOT_A", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD_ONE", "A")});
    w.connect(builder.getOutputPin<ConstantValue<8>, 8>("CONST_ONE", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD_ONE", "B")});
    w.connect(builder.getOutputPin<ConstantValue<1>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Adder8>("ADD_ONE", "Cin")});
    connectAdderResult(builder, *this, "ADD_ONE", "Result");
    w.connect(builder.getOutputPin<ZeroDetect8>("ZERO_DETECT", "ZERO"),
        {builder.getInputPin<NOTGate>("NOT_ZERO", "IN")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_ZERO", "OUT"),
        {getOutputPin("Cout")});
    w.finish();
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
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOT8>("NOT_B");
    builder.addNewComponent<Adder8>("ADD");
    builder.addNewComponent<ConstantValue<1>>("CONST_HIGH", 1);
    buildSubOverflow(builder, *this, "ADD");

    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_A_SPLIT", "IN")});
    w.connect(getInputPin<8>("B"),
        {builder.getInputPin<NOT8, 8>("NOT_B", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_B_SPLIT", "IN")});
    w.connect(builder.getOutputPin<NOT8, 8>("NOT_B", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    w.connect(builder.getOutputPin<ConstantValue<1>>("CONST_HIGH", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    w.connect(builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Cout")});
    w.finish();
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
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOT8>("NOT_B");
    builder.addNewComponent<NOTGate>("NOT_BORROW");
    builder.addNewComponent<Adder8>("ADD");
    buildSubOverflow(builder, *this, "ADD");

    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_A_SPLIT", "IN")});
    w.connect(getInputPin<8>("B"),
        {builder.getInputPin<NOT8, 8>("NOT_B", "A"),
         builder.getInputPin<BitSplitter<8>, 8>("SUB_OVERFLOW_B_SPLIT", "IN")});
    w.connect(getInputPin("Bin"),
        {builder.getInputPin<NOTGate>("NOT_BORROW", "IN")});
    w.connect(builder.getOutputPin<NOT8, 8>("NOT_B", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_BORROW", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    w.connect(builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Bout")});
    w.finish();
}

Incrementer8::Incrementer8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Incrementer8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<Adder8>("ADD");
    builder.addNewComponent<ConstantValue<8>>("CONST_ZERO", 0x00);
    builder.addNewComponent<ConstantValue<1>>("CONST_HIGH", 1);
    buildEqualityFlag(builder, *this, 0x7F, "EQ_7F", "CONST_7F", "Overflow");

    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_7F", "A")});
    w.connect(builder.getOutputPin<ConstantValue<8>, 8>("CONST_ZERO", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    w.connect(builder.getOutputPin<ConstantValue<1>>("CONST_HIGH", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    connectAdderResult(builder, *this, "ADD", "Result");
    w.connect(builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Cout")});
    w.finish();
}

Decrementer8::Decrementer8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Bout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Decrementer8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<Adder8>("ADD");
    builder.addNewComponent<ConstantValue<8>>("CONST_FF", 0xFF);
    builder.addNewComponent<ConstantValue<1>>("CONST_LOW", 0);
    buildEqualityFlag(builder, *this, 0x80, "EQ_80", "CONST_80", "Overflow");

    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<Adder8, 8>("ADD", "A"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_80", "A")});
    w.connect(builder.getOutputPin<ConstantValue<8>, 8>("CONST_FF", "OUT"),
        {builder.getInputPin<Adder8, 8>("ADD", "B")});
    w.connect(builder.getOutputPin<ConstantValue<1>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Adder8>("ADD", "Cin")});
    connectAdderResult(builder, *this, "ADD", "Result");
    w.connect(builder.getOutputPin<Adder8>("ADD", "Cout"),
        {getOutputPin("Bout")});
    w.finish();
}
