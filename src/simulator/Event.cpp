#include "simulator/Event.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "simulator/Simulator.hpp"

Event::Event(SimulationTime event_time) : time(event_time) {}

ComponentEvalEvent::ComponentEvalEvent(SimulationTime event_time, std::shared_ptr<Component> component)
    : Event(event_time), component_to_evaluate(std::move(component)) {}

void ComponentEvalEvent::process(Simulator& sim) {
    if (auto evaluated = std::dynamic_pointer_cast<BasicComponent>(component_to_evaluate)) {
        evaluated->evaluateAt(time, sim);
    }
}

namespace {
size_t widthOf(const std::shared_ptr<WireBase>& wire) { return wire ? wire->getWidth() : 0; }
} // namespace

WireUpdateEvent::WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, LogicValue value)
    : Event(event_time), wire_to_update(std::move(wire)) {
    if (widthOf(wire_to_update)) new_value.set(0, value);
}

WireUpdateEvent::WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, uint64_t value)
    : Event(event_time), wire_to_update(std::move(wire)),
      new_value(PackedLogic::fromUInt64(value, widthOf(wire_to_update))) {}

WireUpdateEvent::WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, const std::vector<LogicValue>& values)
    : Event(event_time), wire_to_update(std::move(wire)),
      new_value(PackedLogic::fromVector(values, widthOf(wire_to_update))) {}

void WireUpdateEvent::process(Simulator& sim) {
    if (!wire_to_update) {
        return;
    }
    // Most updates restate the current value.
    if (wire_to_update->packed() == new_value) {
        return;
    }
    wire_to_update->setPacked(new_value);
    // Histories and traces take a value vector; a headless run only counts.
    if (sim.changeValuesObserved()) sim.recordChange(time, wire_to_update, new_value.toVector(wire_to_update->getWidth()));
    else sim.countEffectiveWireChange();
    wire_to_update->propagateChange(sim, time);
}

void processWireUpdateEvent(SimulationTime time, const std::shared_ptr<WireBase>& wire,
                            const std::vector<LogicValue>& values, Simulator& sim) {
    if (!wire) {
        return;
    }
    if (wire->getValueVector() == values) {
        return;
    }
    wire->setValueVector(values);
    sim.recordChange(time, wire, values);
    wire->propagateChange(sim, time);
}

std::shared_ptr<Event> makeWireUpdateEvent(SimulationTime time, const std::shared_ptr<WireBase>& wire, const std::vector<LogicValue>& values) {
    if (!wire) {
        return nullptr;
    }
    return circuit::makeEvent<WireUpdateEvent>(time, wire, values);
}
