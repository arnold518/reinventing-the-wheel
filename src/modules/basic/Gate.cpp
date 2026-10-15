#include "modules/basic/Gate.hpp"
#include "components/PinMacros.hpp"
#include "simulator/Simulator.hpp"
#include "components/TimingProfile.hpp"

// ========== NOTGate ==========

BEGIN_BASIC_PINS(NOTGate, circuit::timing::leafDelay(circuit::timing::Leaf::Not))
    INPUT_PIN("IN")
    OUTPUT_PIN("OUT")
END_BASIC_PINS()

void NOTGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input = getInputValue("IN");
    LogicValue new_output_value = (input == LogicValue::HIGH) ? LogicValue::LOW :
                                  (input == LogicValue::LOW)  ? LogicValue::HIGH : LogicValue::UNKNOWN;
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// ========== ANDGate ==========

BEGIN_BASIC_PINS(ANDGate, circuit::timing::leafDelay(circuit::timing::Leaf::And))
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("OUT")
END_BASIC_PINS()

void ANDGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");
    LogicValue new_output_value = LogicValue::UNKNOWN;

    if (input_A == LogicValue::LOW || input_B == LogicValue::LOW) {
        new_output_value = LogicValue::LOW;
    } else if (input_A == LogicValue::HIGH && input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::HIGH;
    }
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// ========== NANDGate ==========

BEGIN_BASIC_PINS(NANDGate, circuit::timing::leafDelay(circuit::timing::Leaf::Nand))
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("OUT")
END_BASIC_PINS()

void NANDGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");
    LogicValue new_output_value = LogicValue::UNKNOWN;

    if (input_A == LogicValue::LOW || input_B == LogicValue::LOW) {
        new_output_value = LogicValue::HIGH;
    } else if (input_A == LogicValue::HIGH && input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::LOW;
    }
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// ========== ORGate ==========

BEGIN_BASIC_PINS(ORGate, circuit::timing::leafDelay(circuit::timing::Leaf::Or))
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("OUT")
END_BASIC_PINS()

void ORGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");
    LogicValue new_output_value = LogicValue::UNKNOWN;

    if (input_A == LogicValue::HIGH || input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::HIGH;
    } else if (input_A == LogicValue::LOW && input_B == LogicValue::LOW) {
        new_output_value = LogicValue::LOW;
    }
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// ========== NORGate ==========

BEGIN_BASIC_PINS(NORGate, circuit::timing::leafDelay(circuit::timing::Leaf::Nor))
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("OUT")
END_BASIC_PINS()

void NORGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");
    LogicValue new_output_value = LogicValue::UNKNOWN;

    if (input_A == LogicValue::HIGH || input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::LOW;
    } else if (input_A == LogicValue::LOW && input_B == LogicValue::LOW) {
        new_output_value = LogicValue::HIGH;
    }
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// ========== XORGate ==========

BEGIN_BASIC_PINS(XORGate, circuit::timing::leafDelay(circuit::timing::Leaf::Xor))
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("OUT")
END_BASIC_PINS()

void XORGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");
    LogicValue new_output_value = LogicValue::UNKNOWN;

    if (input_A == LogicValue::UNKNOWN || input_A == LogicValue::HIGH_Z ||
        input_B == LogicValue::UNKNOWN || input_B == LogicValue::HIGH_Z) {
        new_output_value = LogicValue::UNKNOWN;
    } else {
        new_output_value = (input_A != input_B) ? LogicValue::HIGH : LogicValue::LOW;
    }
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}
