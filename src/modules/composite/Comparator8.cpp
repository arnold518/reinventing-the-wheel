#include "modules/composite/Comparator8.hpp"
#include <cstdint>

namespace {
LogicValue flag(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}
}

EqualityChecker8::EqualityChecker8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("EQ", PinType::OUTPUT);
    }) {}

void EqualityChecker8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = getInputValueAsUInt64("A") & 0xFFU;
    auto b = getInputValueAsUInt64("B") & 0xFFU;
    _updateOutputWire(simulator, "EQ", flag(a == b), current_time);
}

Comparator8::Comparator8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("LT", PinType::OUTPUT);
        self->addPin("GT", PinType::OUTPUT);
        self->addPin("EQ", PinType::OUTPUT);
    }) {}

void Comparator8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = getInputValueAsUInt64("A") & 0xFFU;
    auto b = getInputValueAsUInt64("B") & 0xFFU;
    _updateOutputWire(simulator, "LT", flag(a < b), current_time);
    _updateOutputWire(simulator, "GT", flag(a > b), current_time);
    _updateOutputWire(simulator, "EQ", flag(a == b), current_time);
}

SignedComparator8::SignedComparator8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("SLT", PinType::OUTPUT);
        self->addPin("SGT", PinType::OUTPUT);
        self->addPin("SEQ", PinType::OUTPUT);
    }) {}

void SignedComparator8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = static_cast<int8_t>(getInputValueAsUInt64("A") & 0xFFU);
    auto b = static_cast<int8_t>(getInputValueAsUInt64("B") & 0xFFU);
    _updateOutputWire(simulator, "SLT", flag(a < b), current_time);
    _updateOutputWire(simulator, "SGT", flag(a > b), current_time);
    _updateOutputWire(simulator, "SEQ", flag(a == b), current_time);
}
