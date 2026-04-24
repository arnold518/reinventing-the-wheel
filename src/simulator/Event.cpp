#include "simulator/Event.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "simulator/Simulator.hpp"

Event::Event(size_t event_time) : time(event_time) {}

ComponentEvalEvent::ComponentEvalEvent(size_t event_time, std::shared_ptr<Component> component)
    : Event(event_time), component_to_evaluate(std::move(component)) {}

void ComponentEvalEvent::process(Simulator& sim) {
    if (auto basic = std::dynamic_pointer_cast<BasicComponent>(component_to_evaluate)) {
        basic->evaluate(time, sim);
    }
}

void processWireUpdateEvent(size_t time, const std::shared_ptr<WireBase>& wire,
                            const std::vector<LogicValue>& values, Simulator& sim) {
    if (!wire) {
        return;
    }
    wire->setValueVector(values);
    sim.recordChange(time, wire, values);
    wire->propagateChange(sim, time);
}
