#include "simulator/Simulator.hpp"
#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "simulator/Event.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace {
void restorePinsFromWire(const std::shared_ptr<WireBase>& wire, const std::vector<LogicValue>& values) {
    if (!wire) {
        return;
    }

    if (auto source = wire->getSourcePinBase()) {
        source->setValueFromVector(values);
    }

    for (const auto& weak_sink : wire->getSinkPinsBase()) {
        if (auto sink = weak_sink.lock()) {
            sink->setValueFromVector(values);
        }
    }
}
}

Simulator::Simulator() : current_time(0) {}

size_t Simulator::getCurrentTime() const { return current_time; }

void Simulator::scheduleEvent(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }
    ++performance_counters_.scheduled_events;
    if (event->getPriority() == EventPriority::WIRE_UPDATE) {
        ++performance_counters_.scheduled_wire_updates;
    } else if (event->getPriority() == EventPriority::COMPONENT_EVAL) {
        ++performance_counters_.scheduled_component_evaluations;
    }
    event->scheduling_order = next_scheduling_order++;
    event_queue.push(std::move(event));
    performance_counters_.maximum_event_queue_depth = std::max(
        performance_counters_.maximum_event_queue_depth,
        event_queue.size());
}

void Simulator::recordProcessedEvent(const std::shared_ptr<Event>& event) noexcept {
    if (!event) {
        return;
    }
    ++performance_counters_.processed_events;
    if (event->getPriority() == EventPriority::WIRE_UPDATE) {
        ++performance_counters_.processed_wire_updates;
    } else if (event->getPriority() == EventPriority::COMPONENT_EVAL) {
        ++performance_counters_.processed_component_evaluations;
    }
}

void Simulator::advanceAndRecord(size_t target_time) {
    while (!event_queue.empty()) {
        auto event = event_queue.top();

        if (event->time > target_time) {
            break;
        }

        event_queue.pop();

        if (event->time > current_time) {
            current_time = event->time;
            scheduled_for_current_time_eval.clear();
        }

        recordProcessedEvent(event);
        event->process(*this);
    }
}

void Simulator::runAndRecord(size_t max_time) {
    _log.clear();
    _pin_log.clear();
    current_time = 0;
    scheduled_for_current_time_eval.clear();
    advanceAndRecord(max_time);
}

DrainResult Simulator::drainUntilIdle(size_t deadline, size_t max_events) {
    size_t processed_events = 0;

    while (!event_queue.empty()) {
        const auto& event = event_queue.top();
        if (event->time > deadline) {
            return {
                DrainStatus::DeadlineReached,
                current_time,
                processed_events,
            };
        }
        if (processed_events >= max_events) {
            return {
                DrainStatus::EventLimitReached,
                current_time,
                processed_events,
            };
        }

        auto next = event;
        event_queue.pop();
        if (next->time > current_time) {
            current_time = next->time;
            scheduled_for_current_time_eval.clear();
        }
        recordProcessedEvent(next);
        next->process(*this);
        ++processed_events;
    }

    return {
        DrainStatus::Idle,
        current_time,
        processed_events,
    };
}

bool Simulator::hasPendingEvents() const noexcept {
    return !event_queue.empty();
}

std::optional<size_t> Simulator::nextEventTime() const {
    if (event_queue.empty()) {
        return std::nullopt;
    }
    return event_queue.top()->time;
}

void Simulator::setCircuitStateAtTime(size_t target_time) {
    if (!history_recording_enabled_) {
        throw std::logic_error(
            "Cannot time-travel while simulator history recording is disabled");
    }
    for (const auto& [wire, history] : _log) {
        auto it = std::upper_bound(history.begin(), history.end(), target_time,
            [](size_t time, const auto& pair) {
                return time < pair.first;
            });

        if (it == history.begin()) {
            std::vector<LogicValue> unknowns(wire->getWidth(), LogicValue::UNKNOWN);
            wire->setValueVector(unknowns);
            restorePinsFromWire(wire, unknowns);
        } else {
            const auto& values = std::prev(it)->second;
            wire->setValueVector(values);
            restorePinsFromWire(wire, values);
        }
    }
    for (const auto& [pin, history] : _pin_log) {
        auto it = std::upper_bound(history.begin(), history.end(), target_time,
            [](size_t time, const auto& pair) {
                return time < pair.first;
            });

        if (it == history.begin()) {
            pin->setValueFromVector(std::vector<LogicValue>(pin->getWidth(), LogicValue::UNKNOWN));
        } else {
            pin->setValueFromVector(std::prev(it)->second);
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
    for (const auto& [_, history] : _pin_log) {
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

    // WireUpdateEvent calls recordChange only after observing an actual value
    // transition, so this counter does not depend on retaining the timeline.
    ++performance_counters_.effective_wire_changes;
    if (!history_recording_enabled_) {
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

void Simulator::recordPinChange(size_t time, std::shared_ptr<PinBase> pin, const std::vector<LogicValue>& value) {
    if (!pin) {
        return;
    }

    if (!history_recording_enabled_) {
        auto& state = _headless_pin_state[std::move(pin)];
        if (state.empty() || state.back().second != value) {
            state.clear();
            state.push_back({time, value});
            ++performance_counters_.effective_pin_changes;
        }
        return;
    }

    auto& history = _pin_log[std::move(pin)];
    if (history.empty() || history.back().second != value) {
        history.push_back({time, value});
        ++performance_counters_.effective_pin_changes;
    }
}

void Simulator::setHistoryRecordingEnabled(bool enabled) {
    if (history_recording_enabled_ == enabled) {
        return;
    }
    _log.clear();
    _pin_log.clear();
    _headless_pin_state.clear();
    history_recording_enabled_ = enabled;
}

bool Simulator::isHistoryRecordingEnabled() const noexcept {
    return history_recording_enabled_;
}

const SimulatorPerformanceCounters& Simulator::getPerformanceCounters() const noexcept {
    return performance_counters_;
}

void Simulator::resetPerformanceCounters() noexcept {
    performance_counters_ = {};
    performance_counters_.maximum_event_queue_depth = event_queue.size();
}

void Simulator::clear() {
    while (!event_queue.empty()) {
        event_queue.pop();
    }
    _log.clear();
    _pin_log.clear();
    _headless_pin_state.clear();
    current_time = 0;
    next_scheduling_order = 0;
    scheduled_for_current_time_eval.clear();
    resetPerformanceCounters();
}
