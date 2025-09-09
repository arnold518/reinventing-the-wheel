#include "simulator/Simulator.hpp"
#include "components/Component.hpp"
#include "simulator/Event.hpp"
#include "basic/Wire.hpp"          // For getID()
#include "basic/Pin.hpp"           // For getName()
#include <iostream>                // For std::cout
#include <algorithm>
#include <set>

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

void Simulator::runAndRecord(size_t max_time) {
    clear();
    std::cout << "[SIM] === Starting Simulation (Max Time: " << max_time << ") ===" << std::endl;
    
    while (!event_queue.empty() && current_time <= max_time) {
        std::shared_ptr<Event> next_event_ptr = event_queue.top();
        event_queue.pop();

        // --- Time Advancement Logic ---
        if (next_event_ptr->time > current_time) {
            std::cout << "[SIM] >>> Advancing time from " << current_time << " to " << next_event_ptr->time << " <<<" << std::endl;
            current_time = next_event_ptr->time;
            scheduled_for_current_time_eval.clear();
        }
        
        if (current_time > max_time) {
            std::cout << "[SIM] Halting: Next event at time " << next_event_ptr->time << " is beyond max_time " << max_time << "." << std::endl;
            break;
        }

        // --- Event Details Logging ---
        std::cout << "[SIM] T=" << current_time << " | Processing Event (Queue size: " << event_queue.size() << ")" << std::endl;
        if (auto eval_event = std::dynamic_pointer_cast<ComponentEvalEvent>(next_event_ptr)) {
            std::cout << "[SIM] |-> Type: ComponentEvalEvent" << std::endl;
            if(eval_event->component_to_evaluate) {
                std::cout << "[SIM] |   - Target: " << eval_event->component_to_evaluate->getID() << std::endl;
            }
            if(eval_event->triggering_pin) {
                std::cout << "[SIM] |   - Caused by Pin: " << eval_event->triggering_pin->getName() << std::endl;
            }
        } else if (auto wire_event = std::dynamic_pointer_cast<WireUpdateEvent>(next_event_ptr)) {
            std::cout << "[SIM] |-> Type: WireUpdateEvent" << std::endl;
            if(wire_event->wire_to_update) {
                std::cout << "[SIM] |   - Target: " << wire_event->wire_to_update->getID() << std::endl;
                std::cout << "[SIM] |   - New Value: " << wire_event->new_value << std::endl;
            }
        }

        // --- Process the Event ---
        next_event_ptr->process(*this);
    }

    std::cout << "[SIM] === Simulation Finished at T=" << current_time << " ===" << std::endl;
    if (event_queue.empty()) {
        std::cout << "[SIM] Reason: Event queue is empty." << std::endl;
    }
}

// void Simulator::runAndRecord(size_t max_time) {
//     clear();
    
//     while (!event_queue.empty() && current_time <= max_time) {
//         std::shared_ptr<Event> next_event_ptr = event_queue.top();
//         event_queue.pop();

//         if (next_event_ptr->time > current_time) {
//             current_time = next_event_ptr->time;
//             scheduled_for_current_time_eval.clear();
//         }
        
//         if (current_time > max_time) break;

//         next_event_ptr->process(*this);
//     }
// }

void Simulator::setCircuitStateAtTime(size_t target_time) {
    for (auto const& [wire_ptr, history] : _log) {
        
        auto it = std::upper_bound(history.begin(), history.end(), target_time, 
            [](size_t time, const auto& pair) {
                return time < pair.first;
            });

        if (it == history.begin()) {
            wire_ptr->setValue(LogicValue::UNKNOWN);
        } else {
            wire_ptr->setValue(std::prev(it)->second);
        }
    }
    current_time = target_time;
}

std::vector<size_t> Simulator::getUniqueTimestamps() const {
    std::set<size_t> timestamps;
    timestamps.insert(0); // Time 0 is always a valid state
    for (const auto& pair : _log) {
        for (const auto& event : pair.second) {
            timestamps.insert(event.first);
        }
    }
    return {timestamps.begin(), timestamps.end()};
}

void Simulator::recordChange(size_t time, std::shared_ptr<Wire> wire, LogicValue value) {
    // Get the history for the wire (creates it if it doesn't exist)
    auto& history = _log[wire];

    // Add the change only if it's the first event or the value is different
    if (history.empty() || history.back().second != value) {
        history.push_back({time, value});
    }
}

void Simulator::clear() {
    // while (!event_queue.empty()) {
    //     event_queue.pop();
    // }
    _log.clear();
    current_time = 0;
}