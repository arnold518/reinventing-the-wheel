#pragma once

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
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
    const size_t time;

    explicit Event(size_t event_time);
    virtual ~Event() = default;
    virtual void process(Simulator& sim) = 0;
    virtual EventPriority getPriority() const = 0;

    struct EventComparator {
        bool operator()(const std::shared_ptr<Event>& a, const std::shared_ptr<Event>& b) const {
            if (a->time != b->time) {
                return a->time > b->time;
            }
            return a->getPriority() > b->getPriority();
        }
    };
};

void processWireUpdateEvent(size_t time, const std::shared_ptr<WireBase>& wire,
                            const std::vector<LogicValue>& values, Simulator& sim);

class ComponentEvalEvent : public Event {
public:
    std::shared_ptr<Component> component_to_evaluate;

    ComponentEvalEvent(size_t event_time, std::shared_ptr<Component> component);
    void process(Simulator& sim) override;
    EventPriority getPriority() const override {
        return EventPriority::COMPONENT_EVAL;
    }
};

template<size_t WIDTH>
class WireUpdateEvent : public Event {
public:
    std::shared_ptr<Wire<WIDTH>> wire_to_update;
    std::array<LogicValue, WIDTH> new_values;

    WireUpdateEvent(size_t event_time, std::shared_ptr<Wire<WIDTH>> wire, LogicValue value);
    WireUpdateEvent(size_t event_time, std::shared_ptr<Wire<WIDTH>> wire, uint64_t value);
    WireUpdateEvent(size_t event_time, std::shared_ptr<Wire<WIDTH>> wire, const std::vector<LogicValue>& values);

    void process(Simulator& sim) override;
    EventPriority getPriority() const override {
        return EventPriority::WIRE_UPDATE;
    }

    LogicValue getSingleValue() const {
        return new_values[0];
    }
};

#include "simulator/Event.tpp"
