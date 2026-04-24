#include "modules/composite/ZeroDetect8.hpp"

ZeroDetect8::ZeroDetect8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin("ZERO", PinType::OUTPUT);
    }) {}

void ZeroDetect8::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire(
        simulator,
        "ZERO",
        (getInputValueAsUInt64("A") & 0xFFU) == 0 ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
}
