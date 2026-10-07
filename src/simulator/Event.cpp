#include "simulator/Event.hpp"
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

    switch (wire->getWidth()) {
        case 1: return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), values);
        case 2: return std::make_shared<WireUpdateEvent<2>>(time, std::dynamic_pointer_cast<Wire<2>>(wire), values);
        case 3: return std::make_shared<WireUpdateEvent<3>>(time, std::dynamic_pointer_cast<Wire<3>>(wire), values);
        case 4: return std::make_shared<WireUpdateEvent<4>>(time, std::dynamic_pointer_cast<Wire<4>>(wire), values);
        case 5: return std::make_shared<WireUpdateEvent<5>>(time, std::dynamic_pointer_cast<Wire<5>>(wire), values);
        case 8: return std::make_shared<WireUpdateEvent<8>>(time, std::dynamic_pointer_cast<Wire<8>>(wire), values);
        case 16: return std::make_shared<WireUpdateEvent<16>>(time, std::dynamic_pointer_cast<Wire<16>>(wire), values);
        case 32: return std::make_shared<WireUpdateEvent<32>>(time, std::dynamic_pointer_cast<Wire<32>>(wire), values);
        default: return nullptr;
    }
}
