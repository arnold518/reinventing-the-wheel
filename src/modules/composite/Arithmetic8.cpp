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
class SubOverflowDetector8 : public BasicComponent {
public:
    explicit SubOverflowDetector8(std::string name)
        : BasicComponent(std::move(name), 1, [](IOComponent* self) {
              self->addPin<8>("A", PinType::INPUT);
              self->addPin<8>("B", PinType::INPUT);
              self->addPin<8>("Result", PinType::INPUT);
              self->addPin("Overflow", PinType::OUTPUT);
          }) {}

    void evaluate(size_t current_time, Simulator& simulator) override {
        auto a = static_cast<uint8_t>(getInputValueAsUInt64("A") & 0xFFU);
        auto b = static_cast<uint8_t>(getInputValueAsUInt64("B") & 0xFFU);
        auto result = static_cast<uint8_t>(getInputValueAsUInt64("Result") & 0xFFU);
        const bool overflow = ((a ^ b) & (a ^ result) & 0x80U) != 0;
        _updateOutputWire(simulator, "Overflow", overflow ? LogicValue::HIGH : LogicValue::LOW, current_time);
    }
};

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
    builder.addNewComponent<SubOverflowDetector8>("SUB_OVERFLOW");

    builder.addNewWire<8>(
        result_source_name + "_to_Result",
        builder.getOutputPin<Adder8, 8>(result_source_name, "Sum"),
        {component.getOutputPin<8>("Result"), builder.getInputPin<SubOverflowDetector8, 8>("SUB_OVERFLOW", "Result")});
    builder.addNewWire(
        "SUB_OVERFLOW_to_Overflow",
        builder.getOutputPin<SubOverflowDetector8>("SUB_OVERFLOW", "Overflow"),
        {component.getOutputPin("Overflow")});
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
         builder.getInputPin<EqualityChecker8, 8>("EQ_80", "A"),
         builder.getInputPin<ConstantValue<8, 8>, 8>("CONST_ONE", "TRIGGER"),
         builder.getInputPin<ConstantValue<1, 8>, 8>("CONST_LOW", "TRIGGER"),
         builder.getInputPin<ConstantValue<8, 8>, 8>("CONST_80", "TRIGGER")});
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
         builder.getInputPin<SubOverflowDetector8, 8>("SUB_OVERFLOW", "A"),
         builder.getInputPin<ConstantValue<1, 8>, 8>("CONST_HIGH", "TRIGGER")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<NOT8, 8>("NOT_B", "A"),
         builder.getInputPin<SubOverflowDetector8, 8>("SUB_OVERFLOW", "B")});
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
         builder.getInputPin<SubOverflowDetector8, 8>("SUB_OVERFLOW", "A")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<NOT8, 8>("NOT_B", "A"),
         builder.getInputPin<SubOverflowDetector8, 8>("SUB_OVERFLOW", "B")});
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
         builder.getInputPin<EqualityChecker8, 8>("EQ_7F", "A"),
         builder.getInputPin<ConstantValue<8, 8>, 8>("CONST_ZERO", "TRIGGER"),
         builder.getInputPin<ConstantValue<1, 8>, 8>("CONST_HIGH", "TRIGGER"),
         builder.getInputPin<ConstantValue<8, 8>, 8>("CONST_7F", "TRIGGER")});
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
         builder.getInputPin<EqualityChecker8, 8>("EQ_80", "A"),
         builder.getInputPin<ConstantValue<8, 8>, 8>("CONST_FF", "TRIGGER"),
         builder.getInputPin<ConstantValue<1, 8>, 8>("CONST_LOW", "TRIGGER"),
         builder.getInputPin<ConstantValue<8, 8>, 8>("CONST_80", "TRIGGER")});
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
