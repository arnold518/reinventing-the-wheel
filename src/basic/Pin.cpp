#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/BasicComponent.hpp"
#include <iostream>

Pin::Pin(std::string name, PinType type, std::shared_ptr<Component> owner_comp)
    : name(std::move(name)), type(type), owner(owner_comp) {}

std::string Pin::getName() const { return name; }
PinType Pin::getType() const { return type; }
std::shared_ptr<Component> Pin::getOwner() const { return owner.lock(); }
std::shared_ptr<Wire> Pin::getExternalWire() const { return external_wire.lock(); }
std::shared_ptr<Wire> Pin::getInternalWire() const { return internal_wire.lock(); }

void Pin::connectExternal(const std::shared_ptr<Wire>& wire) {
    if (!wire) return;
    external_wire = wire;
}

void Pin::connectInternal(const std::shared_ptr<Wire>& wire) {
    if (!wire) return;
    internal_wire = wire;
}

LogicValue Pin::getValue() const {
    auto owner_ptr = owner.lock();
    if (!owner_ptr) return LogicValue::UNKNOWN;

    // Check if the owner is a primitive component.
    bool is_primitive = (std::dynamic_pointer_cast<BasicComponent>(owner_ptr) != nullptr);
    bool external_first = true;

    if (type == PinType::OUTPUT && !is_primitive) external_first = false;

    if (external_first) {
        if (auto wire = external_wire.lock()) {
            return wire->getValue();
        }
        if (auto wire = internal_wire.lock()) {
            return wire->getValue();
        }
    }
    else {
        if (auto wire = internal_wire.lock()) {
            return wire->getValue();
        }
        if (auto wire = external_wire.lock()) {
            return wire->getValue();
        }
    }
    
    return LogicValue::UNKNOWN;
}

std::string Pin::getID() const {
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + "." + name;
    }
    return "OrphanPin:" + name;
}