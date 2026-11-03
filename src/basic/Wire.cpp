#include "basic/WireBase.hpp"
#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>


WireBase::WireBase(std::string wire_name, size_t width)
    : name(std::move(wire_name)), value_(PackedLogic::allUnknown(width)), width_(width) {
    if (width == 0 || width > PackedLogic::kMaxWidth) throw std::invalid_argument("Wire width must be 1 to 64 bits");
}

LogicValue WireBase::getBit(size_t index) const {
    if (index >= width_) throw std::out_of_range("Wire bit index out of range");
    return value_.get(index);
}

void WireBase::setBit(size_t index, LogicValue bit_value) {
    if (index >= width_) throw std::out_of_range("Wire bit index out of range");
    value_.set(index, bit_value);
}

void WireBase::propagateChange(Simulator& simulator, SimulationTime propagation_time) {
    propagateWireChange(*this, simulator, propagation_time);
}

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

namespace {
// Drops released sinks and makes the cached routes match the sink list.
std::vector<std::weak_ptr<PinBase>>& refreshSinkRoutes(WireBase& wire) {
    auto& sinks = const_cast<std::vector<std::weak_ptr<PinBase>>&>(wire.getSinkPinsBase());
    sinks.erase(std::remove_if(sinks.begin(), sinks.end(),
                               [](const std::weak_ptr<PinBase>& pin) { return pin.expired(); }),
                sinks.end());

    // A pin's owner never changes, so its classification is cached; the
    // cache is valid while it lists the same sink pins in the same order.
    auto& routes = wire.sinkRoutes();
    bool valid = routes.size() == sinks.size();
    for (size_t i = 0; valid && i < sinks.size(); ++i)
        valid = !routes[i].pin.owner_before(sinks[i]) && !sinks[i].owner_before(routes[i].pin);
    if (!valid) {
        routes.clear();
        routes.reserve(sinks.size());
        for (const auto& weak_pin : sinks) {
            WireBase::SinkRoute route{weak_pin, {}, false};
            if (auto pin = weak_pin.lock())
                if (auto owner = pin->getOwner()) {
                    if (auto evaluated = std::dynamic_pointer_cast<BasicComponent>(owner)) route.evaluated = evaluated;
                    else route.forwards = std::dynamic_pointer_cast<IOComponent>(owner) != nullptr;
                }
            routes.push_back(std::move(route));
        }
    }

    return sinks;
}
} // namespace

void propagateWireChange(WireBase& wire, Simulator& simulator, SimulationTime propagation_time) {
    auto& sinks = refreshSinkRoutes(wire);
    const auto& routes = wire.sinkRoutes();
    const PackedLogic value = wire.packed();
    const size_t width = wire.getWidth();
    for (size_t index = 0; index < sinks.size(); ++index) {
        auto sink_pin = sinks[index].lock();
        if (!sink_pin) {
            continue;
        }

        // A sink of another width sees the value truncated or padded with
        // UNKNOWN.
        const size_t sink_width = sink_pin->getWidth();
        sink_pin->setPacked(sink_width == width ? value : value.resized(width, sink_width));
        const auto& route = routes[index];
        if (auto evaluated = route.evaluated.lock()) {
            if (sink_pin->getType() == PinType::INPUT) {
                simulator.scheduleEvent(circuit::makeEvent<ComponentEvalEvent>(propagation_time, evaluated));
            }
            continue;
        }

        // A released owner, or one that is neither a basic part nor a composite.
        if (!route.forwards || !sink_pin->getOwner()) {
            continue;
        }

        auto target_wire = sink_pin->getType() == PinType::INPUT
            ? sink_pin->getInternalWireBase()
            : sink_pin->getExternalWireBase();
        if (target_wire && target_wire.get() != &wire) {
            const size_t target_width = target_wire->getWidth();
            simulator.scheduleEvent(circuit::makeEvent<WireUpdateEvent>(propagation_time, target_wire,
                target_width == width ? value : value.resized(width, target_width)));
        }
    }
}
