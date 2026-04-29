#include "modules/composite/Shifter8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/utility/Rewire.hpp"
#include <vector>

namespace {
void connectRoute(IOComponent& component, ComponentBuilder& builder) {
    builder.addNewWire<8>(
        "A_bus_internal",
        component.getInputPin<8>("A"),
        {builder.getInputPin<Rewire, 8>("ROUTE", "A")});
    builder.addNewWire<8>(
        "Result_bus_internal",
        builder.getOutputPin<Rewire, 8>("ROUTE", "Result"),
        {component.getOutputPin<8>("Result")});
    builder.addNewWire(
        "Carry_internal",
        builder.getOutputPin<Rewire>("ROUTE", "Carry"),
        {component.getOutputPin("Carry")});
}
}

ShiftLeftLogical8::ShiftLeftLogical8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Carry", PinType::OUTPUT);
    }) {}

void ShiftLeftLogical8::buildInternals(ComponentBuilder& builder) {
    std::vector<Rewire::BitMap> mappings;
    for (size_t i = 0; i < 7; ++i) {
        mappings.emplace_back("A", i, "Result", i + 1);
    }
    mappings.emplace_back("A", 7, "Carry", 0);

    builder.addNewComponent<Rewire>(
        "ROUTE",
        std::vector<Rewire::WireSpec>{{"A", 8}},
        std::vector<Rewire::WireSpec>{{"Result", 8}, {"Carry", 1}},
        mappings,
        Rewire::UnmappedBitValue::LOW);
    connectRoute(*this, builder);
}

ShiftRightLogical8::ShiftRightLogical8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Carry", PinType::OUTPUT);
    }) {}

void ShiftRightLogical8::buildInternals(ComponentBuilder& builder) {
    std::vector<Rewire::BitMap> mappings;
    for (size_t i = 1; i < 8; ++i) {
        mappings.emplace_back("A", i, "Result", i - 1);
    }
    mappings.emplace_back("A", 0, "Carry", 0);

    builder.addNewComponent<Rewire>(
        "ROUTE",
        std::vector<Rewire::WireSpec>{{"A", 8}},
        std::vector<Rewire::WireSpec>{{"Result", 8}, {"Carry", 1}},
        mappings,
        Rewire::UnmappedBitValue::LOW);
    connectRoute(*this, builder);
}

ShiftRightArithmetic8::ShiftRightArithmetic8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Carry", PinType::OUTPUT);
    }) {}

void ShiftRightArithmetic8::buildInternals(ComponentBuilder& builder) {
    std::vector<Rewire::BitMap> mappings;
    for (size_t i = 1; i < 8; ++i) {
        mappings.emplace_back("A", i, "Result", i - 1);
    }
    mappings.emplace_back("A", 7, "Result", 7);
    mappings.emplace_back("A", 0, "Carry", 0);

    builder.addNewComponent<Rewire>(
        "ROUTE",
        std::vector<Rewire::WireSpec>{{"A", 8}},
        std::vector<Rewire::WireSpec>{{"Result", 8}, {"Carry", 1}},
        mappings,
        Rewire::UnmappedBitValue::LOW);
    connectRoute(*this, builder);
}
