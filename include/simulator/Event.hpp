#pragma once
#include <memory>
#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"

class Event {
public:
    size_t time;
    
    Event(size_t t) : time(t) {}
    virtual ~Event() = default;

    virtual void process(Simulator& sim) = 0;

    bool operator>(const Event& other) const {
        if (time != other.time) {
            return time > other.time;
        }
        return this > &other;
    }
};

struct EventPtrGreater {
    bool operator()(const std::shared_ptr<Event>& a, const std::shared_ptr<Event>& b) const {
        return *a > *b;
    }
};

class ComponentEvalEvent : public Event {
public:
    std::shared_ptr<Component> component_to_evaluate;

    ComponentEvalEvent(size_t t, std::shared_ptr<Component> comp)
        : Event(t)
        , component_to_evaluate(comp) {}

    void process(Simulator& sim) override;
};

class WireUpdateEvent : public Event {
public:
    std::shared_ptr<Wire> wire_to_update;
    LogicValue new_value;

    WireUpdateEvent(size_t t, std::shared_ptr<Wire> wire, LogicValue val)
        : Event(t)
        , wire_to_update(wire), new_value(val) {}

    void process(Simulator& sim) override;
};