#include "modules/composite/Comparator8.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include <cstdint>

namespace {
class SignedCompareDetector8 : public BasicComponent {
public:
    explicit SignedCompareDetector8(std::string name)
        : BasicComponent(std::move(name), 1, [](IOComponent* self) {
              self->addPin<8>("A", PinType::INPUT);
              self->addPin<8>("B", PinType::INPUT);
              self->addPin("SLT", PinType::OUTPUT);
              self->addPin("SGT", PinType::OUTPUT);
              self->addPin("SEQ", PinType::OUTPUT);
          }) {}

    void evaluate(size_t current_time, Simulator& simulator) override {
        auto a = static_cast<int8_t>(getInputValueAsUInt64("A") & 0xFFU);
        auto b = static_cast<int8_t>(getInputValueAsUInt64("B") & 0xFFU);
        _updateOutputWire(simulator, "SLT", a < b ? LogicValue::HIGH : LogicValue::LOW, current_time);
        _updateOutputWire(simulator, "SGT", a > b ? LogicValue::HIGH : LogicValue::LOW, current_time);
        _updateOutputWire(simulator, "SEQ", a == b ? LogicValue::HIGH : LogicValue::LOW, current_time);
    }
};
}

EqualityChecker8::EqualityChecker8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("EQ", PinType::OUTPUT);
    }) {}

void EqualityChecker8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<XOR8>("XOR");
    builder.addNewComponent<ZeroDetect8>("ZERO");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<XOR8, 8>("XOR", "A")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<XOR8, 8>("XOR", "B")});
    builder.addNewWire<8>(
        "XOR_to_ZERO",
        builder.getOutputPin<XOR8, 8>("XOR", "OUT"),
        {builder.getInputPin<ZeroDetect8, 8>("ZERO", "A")});
    builder.addNewWire(
        "ZERO_to_EQ",
        builder.getOutputPin<ZeroDetect8>("ZERO", "ZERO"),
        {getOutputPin("EQ")});
}

Comparator8::Comparator8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("LT", PinType::OUTPUT);
        self->addPin("GT", PinType::OUTPUT);
        self->addPin("EQ", PinType::OUTPUT);
    }) {}

void Comparator8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Subtractor8>("SUB");
    builder.addNewComponent<EqualityChecker8>("EQ_CHECK");
    builder.addNewComponent<NOTGate>("NOT_COUT");
    builder.addNewComponent<NOTGate>("NOT_EQ");
    builder.addNewComponent<ANDGate>("AND_GT");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<Subtractor8, 8>("SUB", "A"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_CHECK", "A")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<Subtractor8, 8>("SUB", "B"),
         builder.getInputPin<EqualityChecker8, 8>("EQ_CHECK", "B")});
    builder.addNewWire(
        "SUB_Cout_fanout",
        builder.getOutputPin<Subtractor8>("SUB", "Cout"),
        {builder.getInputPin<NOTGate>("NOT_COUT", "IN"), builder.getInputPin<ANDGate>("AND_GT", "A")});
    builder.addNewWire(
        "NOT_COUT_to_LT",
        builder.getOutputPin<NOTGate>("NOT_COUT", "OUT"),
        {getOutputPin("LT")});
    builder.addNewWire(
        "EQ_fanout",
        builder.getOutputPin<EqualityChecker8>("EQ_CHECK", "EQ"),
        {getOutputPin("EQ"), builder.getInputPin<NOTGate>("NOT_EQ", "IN")});
    builder.addNewWire(
        "NOT_EQ_to_AND_GT",
        builder.getOutputPin<NOTGate>("NOT_EQ", "OUT"),
        {builder.getInputPin<ANDGate>("AND_GT", "B")});
    builder.addNewWire(
        "AND_GT_to_GT",
        builder.getOutputPin<ANDGate>("AND_GT", "OUT"),
        {getOutputPin("GT")});
}

SignedComparator8::SignedComparator8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("SLT", PinType::OUTPUT);
        self->addPin("SGT", PinType::OUTPUT);
        self->addPin("SEQ", PinType::OUTPUT);
    }) {}

void SignedComparator8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<SignedCompareDetector8>("SIGNED_COMPARE");
    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<SignedCompareDetector8, 8>("SIGNED_COMPARE", "A")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<SignedCompareDetector8, 8>("SIGNED_COMPARE", "B")});
    builder.addNewWire(
        "SLT_internal",
        builder.getOutputPin<SignedCompareDetector8>("SIGNED_COMPARE", "SLT"),
        {getOutputPin("SLT")});
    builder.addNewWire(
        "SGT_internal",
        builder.getOutputPin<SignedCompareDetector8>("SIGNED_COMPARE", "SGT"),
        {getOutputPin("SGT")});
    builder.addNewWire(
        "SEQ_internal",
        builder.getOutputPin<SignedCompareDetector8>("SIGNED_COMPARE", "SEQ"),
        {getOutputPin("SEQ")});
}
