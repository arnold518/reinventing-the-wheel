#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/Component.hpp"
#include <utility>

PinBase::PinBase(std::string pin_name, PinType pin_type, std::shared_ptr<Component> owner_comp)
    : name(std::move(pin_name)), type(pin_type), owner(std::move(owner_comp)) {}

std::string PinBase::getName() const { return name; }
PinType PinBase::getType() const { return type; }
std::shared_ptr<Component> PinBase::getOwner() const { return owner.lock(); }
std::shared_ptr<WireBase> PinBase::getExternalWireBase() const { return external_wire.lock(); }
std::shared_ptr<WireBase> PinBase::getInternalWireBase() const { return internal_wire.lock(); }

void PinBase::connectExternalBase(const std::shared_ptr<WireBase>& wire) {
    external_wire = wire;
}

void PinBase::connectInternalBase(const std::shared_ptr<WireBase>& wire) {
    internal_wire = wire;
}

LogicValue PinBase::getValue() const {
    return getBit(0);
}

std::string PinBase::getID() const {
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + "." + name;
    }
    return "OrphanPin:" + name;
}
