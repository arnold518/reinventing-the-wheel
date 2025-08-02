#include "basic/Wire.hpp"
#include "simulator/Simulator.hpp"
#include "basic/Pin.hpp"
#include "components/Component.hpp"
#include "simulator/Event.hpp" 
#include <iostream>
#include <algorithm>

Wire::Wire(std::string name) : name(std::move(name)), value(LogicValue::UNKNOWN) {}

std::string Wire::getName() const { return name; }
LogicValue Wire::getValue() const { return value; }

void Wire::setValue(LogicValue new_value) {
    this->value = new_value;
}

void Wire::propagateChange(Simulator& simulator, size_t propagation_time) {
    sink_pins.erase(std::remove_if(sink_pins.begin(), sink_pins.end(),
                                   [](const std::weak_ptr<Pin>& p) { return p.expired(); }),
                    sink_pins.end());

    for (auto const& sink_pin_weak_ptr : sink_pins) {
        if (auto sink_pin = sink_pin_weak_ptr.lock()) {
            if (auto sink_comp = sink_pin->getOwner()) {
                simulator.scheduleEvent(std::make_shared<ComponentEvalEvent>(propagation_time, sink_comp));
            } else {
                std::cerr << "Warning: Sink pin '" << sink_pin->getName() << "' (connected to wire '" << name << "') has a destroyed owner component." << std::endl;
            }
        } else {
            std::cerr << "Warning: Expired sink pin found on wire '" << name << "'. It has been removed." << std::endl;
        }
    }
}

void Wire::setSourcePin(std::shared_ptr<Pin> pin) {
    if (pin && pin->getType() == PinType::OUTPUT) {
        source_pin = pin;
    } else if (pin) {
        std::cerr << "Error: Attempted to set a non-output pin '" << pin->getName() << "' as source for wire '" << name << "'." << std::endl;
    } else {
        std::cerr << "Error: Attempted to set a null pin as source for wire '" << name << "'." << std::endl;
    }
}

void Wire::addSinkPin(std::shared_ptr<Pin> pin) {
    if (pin && pin->getType() == PinType::INPUT) {
        sink_pins.push_back(pin);
    } else if (pin) {
        std::cerr << "Error: Attempted to add a non-input pin '" << pin->getName() << "' as sink for wire '" << name << "'." << std::endl;
    } else {
        std::cerr << "Error: Attempted to add a null pin as sink for wire '" << name << "'." << std::endl;
    }
}

std::shared_ptr<Pin> Wire::getSourcePin() const { return source_pin.lock(); }
const std::vector<std::weak_ptr<Pin>>& Wire::getSinkPins() const { return sink_pins; }

void Wire::setOwner(std::shared_ptr<Component> component)
{
    if(component) {
        owner = component;
    }
    else {
        std::cerr << "Error: Attempted to set a null component as owner for wire '" << name << "'." << std::endl;
    }
}

std::shared_ptr<Component> Wire::getOwner() const { return owner.lock(); }

std::string Wire::getID() const {
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + " > Wire:" + name;
    } else {
        return "OrphanWire:" + name;
    }
}