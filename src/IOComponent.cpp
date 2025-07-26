#include "IOComponent.hpp"
#include "Simulator.hpp"
#include "Pin.hpp"
#include "Wire.hpp"
#include "Event.hpp"
#include <iostream>
#include <typeinfo>

IOComponent::IOComponent(std::string name, size_t delay_val) : Component(std::move(name)), delay(delay_val) {}

std::shared_ptr<Pin> IOComponent::_addPin(const std::string& pin_name, PinType type, std::shared_ptr<IOComponent> self_ptr) {
    auto pin = std::make_shared<Pin>(pin_name, type, self_ptr);
    if (type == PinType::INPUT) {
        _inputPins[pin_name] = pin;
    } else {
        _outputPins[pin_name] = pin;
    }
    return pin;
}

std::shared_ptr<Pin> IOComponent::getInputPin(const std::string& pin_name) const {
    auto it = _inputPins.find(pin_name);
    return (it != _inputPins.end()) ? it->second : nullptr;
}

std::shared_ptr<Pin> IOComponent::getOutputPin(const std::string& pin_name) const {
    auto it = _outputPins.find(pin_name);
    return (it != _outputPins.end()) ? it->second : nullptr;
}

LogicValue IOComponent::getInputValue(const std::string& pin_name) const {
    if (auto pin = getInputPin(pin_name)) {
        return pin->getValue();
    }
    return LogicValue::UNKNOWN;
}

void IOComponent::_updateOutputWire(Simulator& simulator, const std::string& pin_name, LogicValue new_value, size_t current_sim_time) {
    if (auto pin = getOutputPin(pin_name)) {
        if (auto wire = pin->getConnectedWire()) {
            simulator.scheduleEvent(std::make_shared<WireUpdateEvent>(current_sim_time + this->delay, wire, new_value));
        } else {
            std::cerr << "Warning: Output pin '" << pin_name << "' on component '" << getID() << "' tried to write to a destroyed wire." << std::endl;
        }
    }
}