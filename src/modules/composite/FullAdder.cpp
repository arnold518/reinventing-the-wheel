#include "modules/composite/FullAdder.hpp"
#include "components/PinMacros.hpp"

BEGIN_BASIC_PINS(FullAdder, 1)
    INPUT_PIN("A")
    INPUT_PIN("B")
    INPUT_PIN("Carry_in")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry_out")
END_BASIC_PINS()

void FullAdder::evaluate(size_t current_time, Simulator& simulator) {
    auto a = getInputValue("A");
    auto b = getInputValue("B");
    auto cin = getInputValue("Carry_in");
    LogicValue sum = LogicValue::UNKNOWN;
    LogicValue carry = LogicValue::UNKNOWN;

    if ((a == LogicValue::LOW || a == LogicValue::HIGH) &&
        (b == LogicValue::LOW || b == LogicValue::HIGH) &&
        (cin == LogicValue::LOW || cin == LogicValue::HIGH)) {
        int total = (a == LogicValue::HIGH ? 1 : 0) +
                    (b == LogicValue::HIGH ? 1 : 0) +
                    (cin == LogicValue::HIGH ? 1 : 0);
        sum = (total & 1) ? LogicValue::HIGH : LogicValue::LOW;
        carry = (total >= 2) ? LogicValue::HIGH : LogicValue::LOW;
    }

    _updateOutputWire(simulator, "Sum", sum, current_time);
    _updateOutputWire(simulator, "Carry_out", carry, current_time);
}
