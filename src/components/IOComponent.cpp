#include "components/IOComponent.hpp"
#include "basic/SupportedWidths.hpp"
#include "simulator/Simulator.hpp"
#include "basic/Pin.hpp"
#include "basic/PinBase.hpp"
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

std::shared_ptr<PinBase> IOComponent::addPinDynamic(const std::string& pin_name, PinType type, size_t width) {
    return circuit::dispatchWidth(width,
        [&]<size_t WIDTH>() -> std::shared_ptr<PinBase> { return addPin<WIDTH>(pin_name, type); },
        []() -> std::shared_ptr<PinBase> { return nullptr; });
}

std::shared_ptr<PinBase> IOComponent::getInputPinDynamic(const std::string& pin_name) const {
    auto it = _allInputPins.find(pin_name);
    return (it != _allInputPins.end()) ? it->second : nullptr;
}

std::shared_ptr<PinBase> IOComponent::getOutputPinDynamic(const std::string& pin_name) const {
    auto it = _allOutputPins.find(pin_name);
    return (it != _allOutputPins.end()) ? it->second : nullptr;
}

LogicValue IOComponent::getInputValue(const std::string& pin_name) const {
    if (auto pin = getInputPin(pin_name)) {
        return pin->getValue();
    }
    return LogicValue::UNKNOWN;
}

uint64_t IOComponent::getInputValueAsUInt64(const std::string& pin_name) const {
    if (auto pin = getInputPinDynamic(pin_name)) {
        return pin->getValueAsUInt64();
    }
    return 0;
}

std::string IOComponent::formatPins(int lvl) const
{
    std::stringstream ss;
    std::string indent(lvl * 2, ' ');
    for (const auto& [pin_name, pin] : _allInputPins) {
        ss << indent << "  (InputPin" << pin->getWidth() << ") '" << pin_name << "' : " << pin->getValueAsUInt64() << "\n";
    }
    for (const auto& [pin_name, pin] : _allOutputPins) {
        ss << indent << "  (OutputPin" << pin->getWidth() << ") '" << pin_name << "' : " << pin->getValueAsUInt64() << "\n";
    }
    return ss.str();
}

const std::map<std::string, std::shared_ptr<PinBase>>& IOComponent::getAllInputPins() const { return _allInputPins; }

const std::map<std::string, std::shared_ptr<PinBase>>& IOComponent::getAllOutputPins() const { return _allOutputPins; }
