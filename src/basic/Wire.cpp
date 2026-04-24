#include "basic/WireBase.hpp"
#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <utility>

namespace {
std::shared_ptr<Event> makeDynamicWireUpdate(size_t time, const std::shared_ptr<WireBase>& wire, const std::vector<LogicValue>& values) {
    if (!wire) {
        return nullptr;
    }

    switch (wire->getWidth()) {
        case 1: return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), values);
        case 2: return std::make_shared<WireUpdateEvent<2>>(time, std::dynamic_pointer_cast<Wire<2>>(wire), values);
        case 3: return std::make_shared<WireUpdateEvent<3>>(time, std::dynamic_pointer_cast<Wire<3>>(wire), values);
        case 4: return std::make_shared<WireUpdateEvent<4>>(time, std::dynamic_pointer_cast<Wire<4>>(wire), values);
        case 8: return std::make_shared<WireUpdateEvent<8>>(time, std::dynamic_pointer_cast<Wire<8>>(wire), values);
        case 16: return std::make_shared<WireUpdateEvent<16>>(time, std::dynamic_pointer_cast<Wire<16>>(wire), values);
        default: return nullptr;
    }
}
}

WireBase::WireBase(std::string wire_name)
    : name(std::move(wire_name)) {}

std::string WireBase::getName() const { return name; }

void WireBase::setOwner(std::shared_ptr<Component> component) {
    owner = std::move(component);
}

std::shared_ptr<Component> WireBase::getOwner() const {
    return owner.lock();
}

void WireBase::setSourcePinBase(std::shared_ptr<PinBase> pin) {
    source_pin = std::move(pin);
}

void WireBase::addSinkPinBase(std::shared_ptr<PinBase> pin) {
    if (pin) {
        sink_pins.push_back(std::move(pin));
    }
}

std::shared_ptr<PinBase> WireBase::getSourcePinBase() const {
    return source_pin.lock();
}

const std::vector<std::weak_ptr<PinBase>>& WireBase::getSinkPinsBase() const {
    return sink_pins;
}

std::vector<std::shared_ptr<PinBase>> WireBase::getSinkPinsBaseForPython() const {
    std::vector<std::shared_ptr<PinBase>> pins;
    pins.reserve(sink_pins.size());
    for (const auto& weak_pin : sink_pins) {
        if (auto pin = weak_pin.lock()) {
            pins.push_back(pin);
        }
    }
    return pins;
}

std::string WireBase::getID() const {
    if (auto owner_comp = owner.lock()) {
        return owner_comp->getID() + "." + name;
    }
    return "OrphanWire:" + name;
}

void propagateWireChange(WireBase& wire, Simulator& simulator, size_t propagation_time) {
    auto& sinks = const_cast<std::vector<std::weak_ptr<PinBase>>&>(wire.getSinkPinsBase());
    sinks.erase(std::remove_if(sinks.begin(), sinks.end(),
                               [](const std::weak_ptr<PinBase>& pin) { return pin.expired(); }),
                sinks.end());

    const auto values = wire.getValueVector();
    for (const auto& weak_pin : sinks) {
        auto sink_pin = weak_pin.lock();
        if (!sink_pin) {
            continue;
        }

        sink_pin->setValueFromVector(values);
        auto sink_owner = sink_pin->getOwner();
        if (!sink_owner) {
            continue;
        }

        if (auto basic = std::dynamic_pointer_cast<BasicComponent>(sink_owner)) {
            if (sink_pin->getType() == PinType::INPUT) {
                simulator.scheduleEvent(std::make_shared<ComponentEvalEvent>(propagation_time, basic));
            }
            continue;
        }

        if (!std::dynamic_pointer_cast<IOComponent>(sink_owner)) {
            continue;
        }

        auto target_wire = sink_pin->getType() == PinType::INPUT
            ? sink_pin->getInternalWireBase()
            : sink_pin->getExternalWireBase();
        if (target_wire && target_wire.get() != &wire) {
            simulator.scheduleEvent(makeDynamicWireUpdate(propagation_time, target_wire, values));
        }
    }
}
