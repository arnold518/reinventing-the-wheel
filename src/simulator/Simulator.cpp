#include "simulator/Simulator.hpp"
#include "components/Component.hpp"
#include "simulator/Event.hpp"
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
}

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