#pragma once
#include "basic/LogicValue.hpp"
#include <memory>

class Simulator;
class Component;
class Wire;

class Event {
public:
    const size_t time;
    explicit Event(size_t event_time);
    virtual ~Event() = default;
    virtual void process(Simulator& sim) = 0;

    struct EventComparator {
        bool operator()(const std::shared_ptr<Event>& a, const std::shared_ptr<Event>& b) const {
            return a->time > b->time;
        }
    };
};

class ComponentEvalEvent : public Event {
public:
    std::shared_ptr<Component> component_to_evaluate;
    ComponentEvalEvent(size_t event_time, std::shared_ptr<Component> component);
    void process(Simulator& sim) override;
};

class WireUpdateEvent : public Event {
public:
    std::shared_ptr<Wire> wire_to_update;
    LogicValue new_value;
    WireUpdateEvent(size_t event_time, std::shared_ptr<Wire> wire, LogicValue value);
    void process(Simulator& sim) override;
};