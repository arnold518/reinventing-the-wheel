#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/BasicComponent.hpp"
#include "basic/Wire.hpp"
#include <iostream>

void ComponentEvalEvent::process(Simulator& sim) {
    if (auto basic_comp = std::dynamic_pointer_cast<BasicComponent>(component_to_evaluate)) {
        std::cout << "Time " << time << ": Evaluating " << basic_comp->getID() << std::endl;
        basic_comp->evaluate(time, sim);
    } else {
        std::cerr << "Error: Scheduled a non-BasicComponent for evaluation: " << component_to_evaluate->getID() << std::endl;
    }
}

void WireUpdateEvent::process(Simulator& sim) {
    if (wire_to_update) {
        LogicValue old_value = wire_to_update->getValue();
        if (old_value != new_value) {
            wire_to_update->setValue(new_value);
            std::cout << "Time " << time << ": Wire " << wire_to_update->getID() 
                      << " changed from " << old_value << " to " << new_value << std::endl;
            
            wire_to_update->propagateChange(sim, time);
        }
    }
}