#include "modules/basic/Gate.hpp"

// ANDGate

ANDGate::ANDGate(std::string name) : BasicComponent(std::move(name), 1) {}

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

// NANDGate

NANDGate::NANDGate(std::string name) : BasicComponent(std::move(name), 1) {}

void NANDGate::initPins(std::shared_ptr<IOComponent> self_ptr) {
    _addPin("A", PinType::INPUT, self_ptr);
    _addPin("B", PinType::INPUT, self_ptr);
    _addPin("OUT", PinType::OUTPUT, self_ptr);
}

void NANDGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");

    LogicValue new_output_value;
    if (input_A == LogicValue::LOW || input_B == LogicValue::LOW) {
        new_output_value = LogicValue::HIGH;
    } else if (input_A == LogicValue::HIGH && input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::LOW;
    } else {
        new_output_value = LogicValue::UNKNOWN;
    }
    
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// ORGate

ORGate::ORGate(std::string name) : BasicComponent(std::move(name), 1) {}

void ORGate::initPins(std::shared_ptr<IOComponent> self_ptr) {
    _addPin("A", PinType::INPUT, self_ptr);
    _addPin("B", PinType::INPUT, self_ptr);
    _addPin("OUT", PinType::OUTPUT, self_ptr);
}

void ORGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");

    LogicValue new_output_value;
    if (input_A == LogicValue::HIGH || input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::HIGH;
    } else if (input_A == LogicValue::LOW && input_B == LogicValue::LOW) {
        new_output_value = LogicValue::LOW;
    } else {
        new_output_value = LogicValue::UNKNOWN;
    }
    
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// NORGate

NORGate::NORGate(std::string name) : BasicComponent(std::move(name), 1) {}

void NORGate::initPins(std::shared_ptr<IOComponent> self_ptr) {
    _addPin("A", PinType::INPUT, self_ptr);
    _addPin("B", PinType::INPUT, self_ptr);
    _addPin("OUT", PinType::OUTPUT, self_ptr);
}

void NORGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");

    LogicValue new_output_value;
    if (input_A == LogicValue::HIGH || input_B == LogicValue::HIGH) {
        new_output_value = LogicValue::LOW;
    } else if (input_A == LogicValue::LOW && input_B == LogicValue::LOW) {
        new_output_value = LogicValue::HIGH;
    } else {
        new_output_value = LogicValue::UNKNOWN;
    }
    
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// XORGate

XORGate::XORGate(std::string name) : BasicComponent(std::move(name), 1) {}

void XORGate::initPins(std::shared_ptr<IOComponent> self_ptr) {
    _addPin("A", PinType::INPUT, self_ptr);
    _addPin("B", PinType::INPUT, self_ptr);
    _addPin("OUT", PinType::OUTPUT, self_ptr);
}

void XORGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input_A = getInputValue("A");
    LogicValue input_B = getInputValue("B");

    LogicValue new_output_value;
    if (input_A == LogicValue::UNKNOWN || input_A == LogicValue::HIGH_Z) {
        new_output_value = LogicValue::UNKNOWN;
    }
    else if (input_B == LogicValue::UNKNOWN || input_B == LogicValue::HIGH_Z) {
        new_output_value = LogicValue::UNKNOWN;
    }
    else if (input_A == input_B) {
        new_output_value = LogicValue::LOW;
    }
    else {
        new_output_value = LogicValue::HIGH;
    }
    
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}