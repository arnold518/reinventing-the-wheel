#pragma once

#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include <utility>

template<size_t WIDTH>
WireUpdateEvent<WIDTH>::WireUpdateEvent(size_t event_time, std::shared_ptr<Wire<WIDTH>> wire, LogicValue value)
    : Event(event_time), wire_to_update(std::move(wire)) {
    new_values.fill(LogicValue::LOW);
    new_values[0] = value;
}

template<size_t WIDTH>
WireUpdateEvent<WIDTH>::WireUpdateEvent(size_t event_time, std::shared_ptr<Wire<WIDTH>> wire, uint64_t value)
    : Event(event_time), wire_to_update(std::move(wire)) {
    for (size_t i = 0; i < WIDTH; ++i) {
        new_values[i] = ((value >> i) & 1U) ? LogicValue::HIGH : LogicValue::LOW;
    }
}

template<size_t WIDTH>
WireUpdateEvent<WIDTH>::WireUpdateEvent(size_t event_time, std::shared_ptr<Wire<WIDTH>> wire, const std::vector<LogicValue>& values)
    : Event(event_time), wire_to_update(std::move(wire)) {
    for (size_t i = 0; i < WIDTH; ++i) {
        new_values[i] = (i < values.size()) ? values[i] : LogicValue::UNKNOWN;
    }
}

template<size_t WIDTH>
void WireUpdateEvent<WIDTH>::process(Simulator& sim) {
    if (!wire_to_update) {
        return;
    }

    const std::vector<LogicValue> values(new_values.begin(), new_values.end());
    processWireUpdateEvent(time, wire_to_update, values, sim);
}
