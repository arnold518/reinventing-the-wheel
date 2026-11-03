#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/Component.hpp"
#include <stdexcept>
#include <utility>

PinBase::PinBase(std::string pin_name, PinType pin_type, std::shared_ptr<Component> owner_comp, size_t width)
    : name(std::move(pin_name)), type(pin_type), owner(std::move(owner_comp)),
      value_(PackedLogic::allUnknown(width)), width_(width) {
    if (width == 0 || width > PackedLogic::kMaxWidth) throw std::invalid_argument("Pin width must be 1 to 64 bits");
}

LogicValue PinBase::getBit(size_t index) const {
    if (index >= width_) throw std::out_of_range("Pin bit index out of range");
    return value_.get(index);
}

void PinBase::setBit(size_t index, LogicValue bit_value) {
    if (index >= width_) throw std::out_of_range("Pin bit index out of range");
    value_.set(index, bit_value);
}

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
    // Input and output names are separate namespaces in IOComponent.
    const std::string direction = type == PinType::INPUT ? "input" : "output";
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + "." + direction + "." + name;
    }
    return "OrphanPin:" + direction + "." + name;
}
