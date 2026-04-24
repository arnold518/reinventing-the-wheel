#pragma once

#include "basic/LogicValue.hpp"
#include "basic/WireBase.hpp"
#include "simulator/Event.hpp"
#include <map>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

class Simulator {
public:
    using WireHistory = std::vector<std::pair<size_t, std::vector<LogicValue>>>;
    using HistoryLog = std::map<std::shared_ptr<WireBase>, WireHistory>;

    Simulator();

    size_t getCurrentTime() const;
    void scheduleEvent(std::shared_ptr<Event> event);

    void runAndRecord(size_t max_time);
    void setCircuitStateAtTime(size_t target_time);
    std::vector<size_t> getUniqueTimestamps() const;
    void recordChange(size_t time, std::shared_ptr<WireBase> wire, const std::vector<LogicValue>& value);
    void recordChange(size_t time, std::shared_ptr<Wire<>> wire, LogicValue value);
    void clear();

private:
    size_t current_time;
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, Event::EventComparator> event_queue;
    std::map<void*, bool> scheduled_for_current_time_eval;
    HistoryLog _log;
};
