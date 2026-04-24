#include "modules/composite/HalfAdder.hpp"
#include "components/PinMacros.hpp"

BEGIN_BASIC_PINS(HalfAdder, 1)
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry")
END_BASIC_PINS()

void HalfAdder::evaluate(size_t current_time, Simulator& simulator) {
    auto a = getInputValue("A");
    auto b = getInputValue("B");
    LogicValue sum = LogicValue::UNKNOWN;
    LogicValue carry = LogicValue::UNKNOWN;

    if ((a == LogicValue::LOW || a == LogicValue::HIGH) &&
        (b == LogicValue::LOW || b == LogicValue::HIGH)) {
        sum = (a != b) ? LogicValue::HIGH : LogicValue::LOW;
        carry = (a == LogicValue::HIGH && b == LogicValue::HIGH) ? LogicValue::HIGH : LogicValue::LOW;
    }

    _updateOutputWire(simulator, "Sum", sum, current_time);
    _updateOutputWire(simulator, "Carry", carry, current_time);
}
