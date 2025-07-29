#pragma once
#include <queue>
#include <vector>
#include <memory>
#include <functional>
#include <iostream>
#include <map>
#include "ForwardDeclarations.hpp"
#include "simulator/Event.hpp"

class Simulator
{
private:
    size_t current_time;
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventPtrGreater> event_queue;
    
    std::vector<std::shared_ptr<Component>> all_components;
    std::vector<std::shared_ptr<Wire>> all_wires;

    std::map<Component*, bool> scheduled_for_current_time_eval; 

public:
    Simulator();

    size_t getCurrentTime() const;

    void addComponent(std::shared_ptr<Component> comp);
    void addWire(std::shared_ptr<Wire> wire);

    void scheduleEvent(std::shared_ptr<Event> event);

    void run(size_t max_time);
};