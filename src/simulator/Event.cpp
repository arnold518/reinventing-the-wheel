#include "simulator/Event.hpp"
#include "basic/SupportedWidths.hpp"
#include "basic/WireBase.hpp"
#include "basic/Wire.hpp"
#include "components/BasicComponent.hpp"
#include "simulator/Simulator.hpp"

Event::Event(size_t event_time) : time(event_time) {}

ComponentEvalEvent::ComponentEvalEvent(size_t event_time, std::shared_ptr<Component> component)
    : Event(event_time), component_to_evaluate(std::move(component)) {}

void ComponentEvalEvent::process(Simulator& sim) {
    if (auto evaluated = std::dynamic_pointer_cast<BasicComponent>(component_to_evaluate)) {
        evaluated->evaluate(time, sim);
    }
}

void processWireUpdateEvent(size_t time, const std::shared_ptr<WireBase>& wire,
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

std::shared_ptr<Event> makeWireUpdateEvent(size_t time, const std::shared_ptr<WireBase>& wire, const std::vector<LogicValue>& values) {
    if (!wire) {
        return nullptr;
    }
    return circuit::dispatchWidth(wire->getWidth(),
        [&]<size_t WIDTH>() -> std::shared_ptr<Event> {
            return std::make_shared<WireUpdateEvent<WIDTH>>(time, std::dynamic_pointer_cast<Wire<WIDTH>>(wire), values);
        },
        []() -> std::shared_ptr<Event> { return nullptr; });
}
