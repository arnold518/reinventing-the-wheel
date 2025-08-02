#include "simulator/Simulator.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"

Simulator::Simulator() : current_time(0) {}

size_t Simulator::getCurrentTime() const { return current_time; }

void Simulator::scheduleEvent(std::shared_ptr<Event> event) {
    if (!event) return;

    if (event->time == current_time) {
        if (auto comp_event = std::dynamic_pointer_cast<ComponentEvalEvent>(event)) {
            if (scheduled_for_current_time_eval.find(comp_event->component_to_evaluate.get()) == scheduled_for_current_time_eval.end()) {
                event_queue.push(event);
                scheduled_for_current_time_eval[comp_event->component_to_evaluate.get()] = true;
            }
        } else {
            event_queue.push(event);
        }
    } else {
        event_queue.push(event);
    }
}

void Simulator::run(size_t max_time) {
    std::cout << "Simulation started. Max Time: " << max_time << std::endl;
    while (!event_queue.empty() && current_time <= max_time) {
        std::shared_ptr<Event> next_event_ptr = event_queue.top();
        event_queue.pop();

        if (next_event_ptr->time > current_time) {
            current_time = next_event_ptr->time;
            scheduled_for_current_time_eval.clear(); 
        }
        
        if (current_time > max_time) break; 

        next_event_ptr->process(*this);
    }
    std::cout << "Simulation finished at time " << current_time << std::endl;
}