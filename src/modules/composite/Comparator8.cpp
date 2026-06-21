#include "modules/composite/Comparator8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/utility/BitAdapter.hpp"

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
    builder.addNewComponent<Subtractor8>("SUB");
    builder.addNewComponent<BitSplitter<8>>("DIFF_SPLIT");
    builder.addNewComponent<ZeroDetect8>("ZERO_DETECT");
    builder.addNewComponent<XORGate>("SIGNED_LT_XOR");
    builder.addNewComponent<NOTGate>("NOT_SLT");
    builder.addNewComponent<NOTGate>("NOT_EQ");
    builder.addNewComponent<ANDGate>("SGT_AND");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<Subtractor8, 8>("SUB", "A")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<Subtractor8, 8>("SUB", "B")});
    builder.addNewWire<8>(
        "SUB_result_fanout",
        builder.getOutputPin<Subtractor8, 8>("SUB", "Result"),
        {builder.getInputPin<BitSplitter<8>, 8>("DIFF_SPLIT", "IN"),
         builder.getInputPin<ZeroDetect8, 8>("ZERO_DETECT", "A")});
    builder.addNewWire(
        "SUB_result_sign_to_SIGNED_LT_XOR",
        builder.getOutputPin<BitSplitter<8>>("DIFF_SPLIT", "OUT_7"),
        {builder.getInputPin<XORGate>("SIGNED_LT_XOR", "A")});
    builder.addNewWire(
        "SUB_overflow_to_SIGNED_LT_XOR",
        builder.getOutputPin<Subtractor8>("SUB", "Overflow"),
        {builder.getInputPin<XORGate>("SIGNED_LT_XOR", "B")});
    builder.addNewWire(
        "SLT_internal",
        builder.getOutputPin<XORGate>("SIGNED_LT_XOR", "OUT"),
        {getOutputPin("SLT"), builder.getInputPin<NOTGate>("NOT_SLT", "IN")});
    builder.addNewWire(
        "SEQ_internal",
        builder.getOutputPin<ZeroDetect8>("ZERO_DETECT", "ZERO"),
        {getOutputPin("SEQ"), builder.getInputPin<NOTGate>("NOT_EQ", "IN")});
    builder.addNewWire(
        "NOT_SLT_to_SGT_AND",
        builder.getOutputPin<NOTGate>("NOT_SLT", "OUT"),
        {builder.getInputPin<ANDGate>("SGT_AND", "A")});
    builder.addNewWire(
        "NOT_EQ_to_SGT_AND",
        builder.getOutputPin<NOTGate>("NOT_EQ", "OUT"),
        {builder.getInputPin<ANDGate>("SGT_AND", "B")});
    builder.addNewWire(
        "SGT_internal",
        builder.getOutputPin<ANDGate>("SGT_AND", "OUT"),
        {getOutputPin("SGT")});
}
