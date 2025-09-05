#pragma once
#include "simulator/Event.hpp"
#include "basic/Wire.hpp"
#include "basic/LogicValue.hpp"
#include <queue>
#include <memory>
#include <map>
#include <vector>
#include <utility>

class Simulator {
public:
    using WireHistory = std::vector<std::pair<size_t, LogicValue>>;
    using HistoryLog = std::map<std::shared_ptr<Wire>, WireHistory>;

    Simulator();
    
    size_t getCurrentTime() const;
    void scheduleEvent(std::shared_ptr<Event> event);
    
    // History and Simulation Control
    void runAndRecord(size_t max_time);
    void setCircuitStateAtTime(size_t target_time);
    std::vector<size_t> getUniqueTimestamps() const;
    void recordChange(size_t time, std::shared_ptr<Wire> wire, LogicValue value);
    void clear();

private:
    size_t current_time;
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, Event::EventComparator> event_queue;
    std::map<void*, bool> scheduled_for_current_time_eval;
    
    HistoryLog _log;
};