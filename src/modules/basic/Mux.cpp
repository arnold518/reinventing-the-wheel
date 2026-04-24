#include "modules/basic/Mux.hpp"
#include <array>

namespace {
LogicValue bit(uint64_t value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

uint64_t selectInput(IOComponent& component, size_t count) {
    auto sel = component.getInputValueAsUInt64("SEL") & 0x0FU;
    if (sel >= count) {
        sel = 0;
    }
    return component.getInputValue("IN" + std::to_string(sel)) == LogicValue::HIGH ? 1 : 0;
}

uint64_t selectInput8(IOComponent& component, size_t count) {
    auto sel = component.getInputValueAsUInt64("SEL") & 0x0FU;
    if (sel >= count) {
        sel = 0;
    }
    return component.getInputValueAsUInt64("IN" + std::to_string(sel)) & 0xFFU;
}
}

Mux2to1::Mux2to1(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin("A", PinType::INPUT);
        self->addPin("B", PinType::INPUT);
        self->addPin("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux2to1::evaluate(size_t current_time, Simulator& simulator) {
    auto out = getInputValue("SEL") == LogicValue::HIGH ? getInputValue("B") : getInputValue("A");
    _updateOutputWire(simulator, "OUT", out, current_time);
}

Mux4to1::Mux4to1(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        for (int i = 0; i < 4; ++i) self->addPin("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<2>("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux4to1::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire(simulator, "OUT", bit(selectInput(*this, 4)), current_time);
}

Mux8to1::Mux8to1(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        for (int i = 0; i < 8; ++i) self->addPin("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<3>("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux8to1::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire(simulator, "OUT", bit(selectInput(*this, 8)), current_time);
}

Mux16to1::Mux16to1(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        for (int i = 0; i < 16; ++i) self->addPin("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<4>("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux16to1::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire(simulator, "OUT", bit(selectInput(*this, 16)), current_time);
}

Mux2to1_8bit::Mux2to1_8bit(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux2to1_8bit::evaluate(size_t current_time, Simulator& simulator) {
    auto out = getInputValue("SEL") == LogicValue::HIGH ? getInputValueAsUInt64("B") : getInputValueAsUInt64("A");
    _updateOutputWire<8>(simulator, "OUT", out & 0xFFU, current_time);
}

Mux4to1_8bit::Mux4to1_8bit(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        for (int i = 0; i < 4; ++i) self->addPin<8>("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<2>("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux4to1_8bit::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", selectInput8(*this, 4), current_time);
}

Mux8to1_8bit::Mux8to1_8bit(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        for (int i = 0; i < 8; ++i) self->addPin<8>("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<3>("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux8to1_8bit::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", selectInput8(*this, 8), current_time);
}

Mux16to1_8bit::Mux16to1_8bit(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        for (int i = 0; i < 16; ++i) self->addPin<8>("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<4>("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux16to1_8bit::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", selectInput8(*this, 16), current_time);
}
