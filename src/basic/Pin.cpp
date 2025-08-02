#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include <iostream>

Pin::Pin(std::string name, PinType type, std::shared_ptr<Component> owner_comp)
    : name(std::move(name)), type(type), owner(owner_comp) {}

std::string Pin::getName() const { return name; }
PinType Pin::getType() const { return type; }

std::shared_ptr<Component> Pin::getOwner() const { return owner.lock(); }
std::shared_ptr<Wire> Pin::getConnectedWire() const { return connected_wire.lock(); }

void Pin::connect(std::shared_ptr<Wire> wire) {
    if (!wire) {
        std::cerr << "Error: Attempting to connect Pin " << name << " to a null Wire." << std::endl;
        return;
    }
    connected_wire = wire;
    if (type == PinType::INPUT) {
        wire->addSinkPin(shared_from_this()); 
    } else {
        wire->setSourcePin(shared_from_this());
    }
}

LogicValue Pin::getValue() const {
    if (type == PinType::INPUT || type == PinType::OUTPUT) {
        if (auto wire = connected_wire.lock()) {
            return wire->getValue();
        } else {
            std::cerr << "Warning: Pin '" << name << "' tried to read from a destroyed wire." << std::endl;
            return LogicValue::UNKNOWN; 
        }
    }
    return LogicValue::UNKNOWN;
}

std::string Pin::getID() const {
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + " > Pin:" + name;
    } else {
        return "OrphanPin:" + name;
    }
}