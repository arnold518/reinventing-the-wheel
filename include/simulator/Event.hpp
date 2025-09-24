#pragma once
#include "basic/LogicValue.hpp"
#include <memory>

class Simulator;
class Component;
class Wire;
class Pin; // Forward declaration for Pin is needed

enum class EventPriority {
    WIRE_UPDATE = 0,    // Highest priority
    COMPONENT_EVAL = 1  // Lower priority
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

class ComponentEvalEvent : public Event {
public:
    std::shared_ptr<Component> component_to_evaluate;

    ComponentEvalEvent(size_t event_time, std::shared_ptr<Component> component);
    void process(Simulator& sim) override;
    EventPriority getPriority() const override {
        return EventPriority::COMPONENT_EVAL;
    }
};

class WireUpdateEvent : public Event {
public:
    std::shared_ptr<Wire> wire_to_update;
    LogicValue new_value;
    WireUpdateEvent(size_t event_time, std::shared_ptr<Wire> wire, LogicValue value);
    void process(Simulator& sim) override;
    EventPriority getPriority() const override {
        return EventPriority::WIRE_UPDATE;
    }
};