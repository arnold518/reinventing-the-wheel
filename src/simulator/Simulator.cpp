#include "simulator/Simulator.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "simulator/Event.hpp"
#include <algorithm>
#include <set>

Simulator::Simulator() : current_time(0) {}

size_t Simulator::getCurrentTime() const { return current_time; }

void Simulator::scheduleEvent(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }

    if (event->time == current_time) {
        if (auto eval = std::dynamic_pointer_cast<ComponentEvalEvent>(event)) {
            void* key = eval->component_to_evaluate.get();
            if (scheduled_for_current_time_eval.find(key) != scheduled_for_current_time_eval.end()) {
                return;
            }
            scheduled_for_current_time_eval[key] = true;
        }
    }
    event_queue.push(std::move(event));
}

void Simulator::runAndRecord(size_t max_time) {
    _log.clear();
    current_time = 0;
    scheduled_for_current_time_eval.clear();

    while (!event_queue.empty()) {
        auto event = event_queue.top();
        event_queue.pop();

        if (event->time > max_time) {
            break;
        }

        if (event->time > current_time) {
            current_time = event->time;
            scheduled_for_current_time_eval.clear();
        }

        event->process(*this);
    }
}

void Simulator::setCircuitStateAtTime(size_t target_time) {
    for (const auto& [wire, history] : _log) {
        auto it = std::upper_bound(history.begin(), history.end(), target_time,
            [](size_t time, const auto& pair) {
                return time < pair.first;
            });

        if (it == history.begin()) {
            std::vector<LogicValue> unknowns(wire->getWidth(), LogicValue::UNKNOWN);
            wire->setValueVector(unknowns);
        } else {
            wire->setValueVector(std::prev(it)->second);
        }
    }
    current_time = target_time;
}

std::vector<size_t> Simulator::getUniqueTimestamps() const {
    std::set<size_t> timestamps;
    timestamps.insert(0);
    for (const auto& [_, history] : _log) {
        for (const auto& [time, value] : history) {
            (void)value;
            timestamps.insert(time);
        }
    }
    return {timestamps.begin(), timestamps.end()};
}

void Simulator::recordChange(size_t time, std::shared_ptr<WireBase> wire, const std::vector<LogicValue>& value) {
    if (!wire) {
        return;
    }

    auto& history = _log[std::move(wire)];
    if (history.empty() || history.back().second != value) {
        history.push_back({time, value});
    }
}

void Simulator::recordChange(size_t time, std::shared_ptr<Wire<>> wire, LogicValue value) {
    recordChange(time, std::static_pointer_cast<WireBase>(std::move(wire)), std::vector<LogicValue>{value});
}

void Simulator::clear() {
    while (!event_queue.empty()) {
        event_queue.pop();
    }
    _log.clear();
    current_time = 0;
    scheduled_for_current_time_eval.clear();
}
