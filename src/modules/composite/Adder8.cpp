#include "modules/composite/Adder8.hpp"

Adder8::Adder8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("Cin", PinType::INPUT);
        self->addPin<8>("Sum", PinType::OUTPUT);
        self->addPin("Cout", PinType::OUTPUT);
    }) {}

void Adder8::evaluate(size_t current_time, Simulator& simulator) {
    uint16_t a = static_cast<uint16_t>(getInputValueAsUInt64("A") & 0xFFU);
    uint16_t b = static_cast<uint16_t>(getInputValueAsUInt64("B") & 0xFFU);
    uint16_t cin = getInputValue("Cin") == LogicValue::HIGH ? 1 : 0;
    uint16_t sum = a + b + cin;

    _updateOutputWire<8>(simulator, "Sum", sum & 0xFFU, current_time);
    _updateOutputWire(simulator, "Cout", sum > 0xFFU ? LogicValue::HIGH : LogicValue::LOW, current_time);
}
