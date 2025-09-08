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
void Wire::setValue(LogicValue new_value) { this->value = new_value; }

void Wire::propagateChange(Simulator& simulator, size_t propagation_time) {
    sink_pins.erase(std::remove_if(sink_pins.begin(), sink_pins.end(),
                                   [](const std::weak_ptr<Pin>& p) { return p.expired(); }),
                    sink_pins.end());

    for (auto const& sink_pin_weak_ptr : sink_pins) {
        if (auto sink_pin = sink_pin_weak_ptr.lock()) {
            if (auto sink_comp = sink_pin->getOwner()) {
                simulator.scheduleEvent(std::make_shared<ComponentEvalEvent>(propagation_time, sink_comp));
            }
        }
    }
}

void Wire::setSourcePin(std::shared_ptr<Pin> pin) {
    if (!pin) {
        std::cerr << "Error: Attempted to set a null pin as source for wire '" << name << "'." << std::endl;
        return;
    }
    // This is the critical safety check to prevent short circuits.
    if (auto existing_source = source_pin.lock()) {
        std::cerr << "Error: Wire '" << name << "' already has a source pin ('" << existing_source->getID() 
                  << "'). Cannot set new source ('" << pin->getID() << "')." << std::endl;
        return;
    }
    source_pin = pin;
}

void Wire::addSinkPin(std::shared_ptr<Pin> pin) {
    if (!pin) {
        std::cerr << "Error: Attempted to add a null pin as sink for wire '" << name << "'." << std::endl;
        return;
    }
    // The wire no longer validates pin type; it simply accepts the connection.
    // The ComponentBuilder is responsible for ensuring only valid pins are passed.
    sink_pins.push_back(pin);
}

std::shared_ptr<Pin> Wire::getSourcePin() const { return source_pin.lock(); }
const std::vector<std::weak_ptr<Pin>>& Wire::getSinkPins() const { return sink_pins; }

void Wire::setOwner(std::shared_ptr<Component> component) {
    if(component) {
        owner = component;
    } else {
        std::cerr << "Error: Attempted to set a null component as owner for wire '" << name << "'." << std::endl;
    }
}

std::shared_ptr<Component> Wire::getOwner() const { return owner.lock(); }

std::string Wire::getID() const {
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + "." + name;
    } else {
        return "OrphanWire:" + name;
    }
}