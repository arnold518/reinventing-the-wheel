#include "Gate.hpp"

ANDGate::ANDGate(std::string name) : BasicComponent(std::move(name), 2) {}

void ANDGate::initPins(std::shared_ptr<IOComponent> self_ptr) {
    _addPin("A", PinType::INPUT, self_ptr);
    _addPin("B", PinType::INPUT, self_ptr);
    _addPin("OUT", PinType::OUTPUT, self_ptr);
}

void ANDGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");

    LogicValue new_output_value;
    if (input_A == LogicValue::LOW || input_B == LogicValue::LOW) {
        new_output_value = LogicValue::LOW;
    } else if (input_A == LogicValue::HIGH && input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::HIGH;
    } else {
        new_output_value = LogicValue::UNKNOWN;
    }
    
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

NANDGate::NANDGate(std::string name) : BasicComponent(std::move(name), 2) {}

void NANDGate::initPins(std::shared_ptr<IOComponent> self_ptr) {
    _addPin("A", PinType::INPUT, self_ptr);
    _addPin("B", PinType::INPUT, self_ptr);
    _addPin("OUT", PinType::OUTPUT, self_ptr);
}

void NANDGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");

    LogicValue and_result;
    if (input_A == LogicValue::LOW || input_B == LogicValue::LOW) {
        and_result = LogicValue::LOW;
    } else if (input_A == LogicValue::HIGH && input_B == LogicValue::HIGH) {
        and_result = LogicValue::HIGH;
    } else {
        and_result = LogicValue::UNKNOWN;
    }

    LogicValue new_output_value;
    if (and_result == LogicValue::HIGH) {
        new_output_value = LogicValue::LOW;
    } else if (and_result == LogicValue::LOW) {
        new_output_value = LogicValue::HIGH;
    } else {
        new_output_value = LogicValue::UNKNOWN;
    }
    
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}