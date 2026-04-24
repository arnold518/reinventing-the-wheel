#include "modules/composite/Arithmetic8.hpp"

namespace {
LogicValue flag(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

bool addOverflow(uint8_t a, uint8_t b, uint8_t result) {
    return ((~(a ^ b) & (a ^ result) & 0x80U) != 0);
}

bool subOverflow(uint8_t a, uint8_t b, uint8_t result) {
    return (((a ^ b) & (a ^ result) & 0x80U) != 0);
}
}

TwosComplement8::TwosComplement8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void TwosComplement8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = static_cast<uint8_t>(getInputValueAsUInt64("A") & 0xFFU);
    auto result = static_cast<uint8_t>((~a + 1U) & 0xFFU);
    _updateOutputWire<8>(simulator, "Result", result, current_time);
    _updateOutputWire(simulator, "Cout", flag(a != 0), current_time);
    _updateOutputWire(simulator, "Overflow", flag(a == 0x80U), current_time);
}

Subtractor8::Subtractor8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Subtractor8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = static_cast<uint8_t>(getInputValueAsUInt64("A") & 0xFFU);
    auto b = static_cast<uint8_t>(getInputValueAsUInt64("B") & 0xFFU);
    uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(~b & 0xFFU) + 1U;
    auto result = static_cast<uint8_t>(sum & 0xFFU);
    _updateOutputWire<8>(simulator, "Result", result, current_time);
    _updateOutputWire(simulator, "Cout", flag(sum > 0xFFU), current_time);
    _updateOutputWire(simulator, "Overflow", flag(subOverflow(a, b, result)), current_time);
}

SubtractorWithBorrow8::SubtractorWithBorrow8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("Bin", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Bout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void SubtractorWithBorrow8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = static_cast<uint8_t>(getInputValueAsUInt64("A") & 0xFFU);
    auto b = static_cast<uint8_t>(getInputValueAsUInt64("B") & 0xFFU);
    uint16_t borrow = getInputValue("Bin") == LogicValue::HIGH ? 1U : 0U;
    uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(~b & 0xFFU) + (1U - borrow);
    auto result = static_cast<uint8_t>(sum & 0xFFU);
    _updateOutputWire<8>(simulator, "Result", result, current_time);
    _updateOutputWire(simulator, "Bout", flag(sum > 0xFFU), current_time);
    _updateOutputWire(simulator, "Overflow", flag(subOverflow(a, b, result)), current_time);
}

Incrementer8::Incrementer8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Incrementer8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = static_cast<uint8_t>(getInputValueAsUInt64("A") & 0xFFU);
    uint16_t sum = static_cast<uint16_t>(a) + 1U;
    auto result = static_cast<uint8_t>(sum & 0xFFU);
    _updateOutputWire<8>(simulator, "Result", result, current_time);
    _updateOutputWire(simulator, "Cout", flag(sum > 0xFFU), current_time);
    _updateOutputWire(simulator, "Overflow", flag(addOverflow(a, 1U, result)), current_time);
}

Decrementer8::Decrementer8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Bout", PinType::OUTPUT);
        self->addPin("Overflow", PinType::OUTPUT);
    }) {}

void Decrementer8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = static_cast<uint8_t>(getInputValueAsUInt64("A") & 0xFFU);
    auto result = static_cast<uint8_t>((a - 1U) & 0xFFU);
    _updateOutputWire<8>(simulator, "Result", result, current_time);
    _updateOutputWire(simulator, "Bout", flag(a != 0), current_time);
    _updateOutputWire(simulator, "Overflow", flag(a == 0x80U), current_time);
}
