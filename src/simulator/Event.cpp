#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "basic/Wire.hpp"
#include "basic/Pin.hpp"
#include <iostream>

Event::Event(size_t event_time) : time(event_time) {}

// --- ComponentEvalEvent Implementation ---

ComponentEvalEvent::ComponentEvalEvent(size_t event_time, std::shared_ptr<Component> component)
    : Event(event_time), 
      component_to_evaluate(std::move(component)) {}

void ComponentEvalEvent::process(Simulator& sim) {
    if (auto basic_comp = std::dynamic_pointer_cast<BasicComponent>(component_to_evaluate)) {
        std::cout << "[COMPONENTEVAL] Evaluating primitive component '" << basic_comp->getID() << "'." << std::endl;
        basic_comp->evaluate(time, sim);
    } else {
        std::cerr << "Error: Scheduled a non-BasicComponent for evaluation: " << component_to_evaluate->getID() << std::endl;
    }
}


// --- WireUpdateEvent Implementation ---

WireUpdateEvent::WireUpdateEvent(size_t event_time, std::shared_ptr<Wire> wire, LogicValue value)
    : Event(event_time), 
      wire_to_update(std::move(wire)), 
      new_value(value) {}

void WireUpdateEvent::process(Simulator& sim) {
    if (wire_to_update) {
        LogicValue old_value = wire_to_update->getValue();
        if (old_value != new_value) {
            wire_to_update->setValue(new_value);
            std::cout << "[WIREUPDATE] Time " << time << ": Wire " << wire_to_update->getID() 
                      << " changed from " << old_value << " to " << new_value << std::endl;
            sim.recordChange(time, wire_to_update, new_value);
            wire_to_update->propagateChange(sim, time);
        }
    }
}