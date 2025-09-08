#include "components/IOComponent.hpp"
#include "simulator/Simulator.hpp"
#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include <iostream>
#include <sstream>

IOComponent::IOComponent(std::string name, PinInitFunction initializer) 
    : Component(std::move(name)), 
      _pin_initializer(std::move(initializer)) {}

void IOComponent::initPins(std::shared_ptr<IOComponent> self_ptr) {
    if (_pin_initializer) {
        _pin_initializer(this);
    }
}

std::shared_ptr<Pin> IOComponent::addPin(const std::string& pin_name, PinType type) {
    auto self_ptr = std::static_pointer_cast<IOComponent>(shared_from_this());
    return this->_addPin(pin_name, type, self_ptr);
}

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

std::string IOComponent::formatPins(int lvl) const
{
    std::stringstream ss;
    std::string indent(lvl * 2, ' ');
    for (const auto& [pin_name, pin] : _inputPins) ss << indent << "  (InputPin) '" << pin_name << "' : " << pin->getValue() << "\n";
    for (const auto& [pin_name, pin] : _outputPins) ss << indent << "  (OutputPin) '" << pin_name << "' : " << pin->getValue() << "\n";
    return ss.str();
}

const std::map<std::string, std::shared_ptr<Pin>>& IOComponent::getInputPins() const { return _inputPins; }

const std::map<std::string, std::shared_ptr<Pin>>& IOComponent::getOutputPins() const { return _outputPins; }