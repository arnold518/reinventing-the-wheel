#include "modules/basic/Gate.hpp"
#include "simulator/Simulator.hpp"

// NOTGate
NOTGate::NOTGate(std::string name)
    : BasicComponent(std::move(name), 1,
        [](IOComponent* self) {
            self->addPin("IN", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}

void NOTGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue input = getInputValue("IN");
    LogicValue new_output_value = (input == LogicValue::HIGH) ? LogicValue::LOW :
                                  (input == LogicValue::LOW)  ? LogicValue::HIGH : LogicValue::UNKNOWN;
    _updateOutputWire(simulator, "OUT", new_output_value, current_time);
}

// ANDGate
ANDGate::ANDGate(std::string name)
    : BasicComponent(std::move(name), 1,
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}

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

// NANDGate
NANDGate::NANDGate(std::string name)
    : BasicComponent(std::move(name), 1,
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}

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

// ORGate
ORGate::ORGate(std::string name)
    : BasicComponent(std::move(name), 1,
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}

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

// NORGate
NORGate::NORGate(std::string name)
    : BasicComponent(std::move(name), 1,
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}

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

// XORGate
XORGate::XORGate(std::string name)
    : BasicComponent(std::move(name), 1,
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}

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