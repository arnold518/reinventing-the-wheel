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

void Pin::connect(const std::shared_ptr<Wire>& wire) {
    if (!wire) return;

    if (auto existing_wire = connected_wire.lock()) {
         if (existing_wire != wire) {
            std::cerr << "Warning: Pin '" << getID() << "' is already connected to a wire. Re-connecting." << std::endl;
         }
    }
    connected_wire = wire;
}

LogicValue Pin::getValue() const {
    if (auto wire = connected_wire.lock()) {
        return wire->getValue();
    }
    return LogicValue::UNKNOWN;
}

std::string Pin::getID() const {
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + "." + name;
    }
    return "OrphanPin:" + name;
}