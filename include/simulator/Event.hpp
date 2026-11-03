#pragma once

#include "simulator/SimulationTime.hpp"

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
#include "basic/PackedLogic.hpp"
#include "simulator/EventPool.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

enum class EventPriority {
    WIRE_UPDATE = 0,
    COMPONENT_EVAL = 1
};

class Event {
public:
    const SimulationTime time;
    uint64_t scheduling_order = 0;
    // Set when an inertial-delay gate withdraws an output change it had
    // scheduled (a pulse shorter than its delay). The simulator drops it.
    bool cancelled = false;

    explicit Event(SimulationTime event_time);
    virtual ~Event() = default;
    virtual void process(Simulator& sim) = 0;
    virtual EventPriority getPriority() const = 0;

    struct EventComparator {
        bool operator()(const std::shared_ptr<Event>& a, const std::shared_ptr<Event>& b) const {
            if (a->time != b->time) {
                return a->time > b->time;
            }
            if (a->getPriority() != b->getPriority()) {
                return a->getPriority() > b->getPriority();
            }
            return a->scheduling_order > b->scheduling_order;
        }
    };
};

void processWireUpdateEvent(SimulationTime time, const std::shared_ptr<WireBase>& wire,
                            const std::vector<LogicValue>& values, Simulator& sim);

// WireUpdateEvent from a value vector (padded with UNKNOWN or truncated to the
// wire's width). Returns nullptr for a missing wire.
std::shared_ptr<Event> makeWireUpdateEvent(SimulationTime time, const std::shared_ptr<WireBase>& wire,
                                           const std::vector<LogicValue>& values);

class ComponentEvalEvent : public Event {
public:
    std::shared_ptr<Component> component_to_evaluate;

    ComponentEvalEvent(SimulationTime event_time, std::shared_ptr<Component> component);
    void process(Simulator& sim) override;
    EventPriority getPriority() const override {
        return EventPriority::COMPONENT_EVAL;
    }
};

// Sets a wire to a new value and, when that changes it, delivers the change to
// the wire's sinks. One class for every width: the value is packed.
class WireUpdateEvent : public Event {
public:
    std::shared_ptr<WireBase> wire_to_update;
    PackedLogic new_value;

    // Bit 0 = value, the other bits LOW.
    WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, LogicValue value);
    // Two-state bits, truncated to the wire's width.
    WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, uint64_t value);
    // Padded with UNKNOWN or truncated to the wire's width.
    WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, const std::vector<LogicValue>& values);
    // Already the wire's width (bits above it zero).
    WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, const PackedLogic& value)
        : Event(event_time), wire_to_update(std::move(wire)), new_value(value) {}
    template<size_t WIDTH>
    WireUpdateEvent(SimulationTime event_time, std::shared_ptr<WireBase> wire, const std::array<LogicValue, WIDTH>& values)
        : WireUpdateEvent(event_time, std::move(wire), std::vector<LogicValue>(values.begin(), values.end())) {}

    void process(Simulator& sim) override;
    EventPriority getPriority() const override {
        return EventPriority::WIRE_UPDATE;
    }

    LogicValue getSingleValue() const {
        return new_value.get(0);
    }
};
