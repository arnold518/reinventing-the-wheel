#include "modules/composite/Shifter8.hpp"

namespace {
LogicValue flag(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}
}

ShiftLeftLogical8::ShiftLeftLogical8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Carry", PinType::OUTPUT);
    }) {}

void ShiftLeftLogical8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = getInputValueAsUInt64("A") & 0xFFU;
    _updateOutputWire<8>(simulator, "Result", (a << 1U) & 0xFFU, current_time);
    _updateOutputWire(simulator, "Carry", flag((a & 0x80U) != 0), current_time);
}

ShiftRightLogical8::ShiftRightLogical8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Carry", PinType::OUTPUT);
    }) {}

void ShiftRightLogical8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = getInputValueAsUInt64("A") & 0xFFU;
    _updateOutputWire<8>(simulator, "Result", (a >> 1U) & 0xFFU, current_time);
    _updateOutputWire(simulator, "Carry", flag((a & 0x01U) != 0), current_time);
}

ShiftRightArithmetic8::ShiftRightArithmetic8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("Result", PinType::OUTPUT);
        self->addPin("Carry", PinType::OUTPUT);
    }) {}

void ShiftRightArithmetic8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = getInputValueAsUInt64("A") & 0xFFU;
    auto result = ((a >> 1U) | (a & 0x80U)) & 0xFFU;
    _updateOutputWire<8>(simulator, "Result", result, current_time);
    _updateOutputWire(simulator, "Carry", flag((a & 0x01U) != 0), current_time);
}
