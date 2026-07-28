#pragma once

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
#include "basic/WireBase.hpp"
#include "simulator/Event.hpp"
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

enum class DrainStatus {
    Idle,
    DeadlineReached,
    EventLimitReached,
};

struct DrainResult {
    DrainStatus status = DrainStatus::Idle;
    size_t final_time = 0;
    size_t processed_events = 0;

    bool idle() const noexcept { return status == DrainStatus::Idle; }
};

class Simulator {
public:
    using WireHistory = std::vector<std::pair<size_t, std::vector<LogicValue>>>;
    using HistoryLog = std::map<std::shared_ptr<WireBase>, WireHistory>;
    using PinHistory = std::vector<std::pair<size_t, std::vector<LogicValue>>>;
    using PinHistoryLog = std::map<std::shared_ptr<PinBase>, PinHistory>;

    Simulator();

    size_t getCurrentTime() const;
    void scheduleEvent(std::shared_ptr<Event> event);

    void advanceAndRecord(size_t target_time);
    void runAndRecord(size_t max_time);
    DrainResult drainUntilIdle(size_t deadline, size_t max_events);
    bool hasPendingEvents() const noexcept;
    std::optional<size_t> nextEventTime() const;
    void setCircuitStateAtTime(size_t target_time);
    std::vector<size_t> getUniqueTimestamps() const;
    void recordChange(size_t time, std::shared_ptr<WireBase> wire, const std::vector<LogicValue>& value);
    void recordChange(size_t time, std::shared_ptr<Wire<>> wire, LogicValue value);
    void recordPinChange(size_t time, std::shared_ptr<PinBase> pin, const std::vector<LogicValue>& value);
    void clear();

private:
    size_t current_time;
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, Event::EventComparator> event_queue;
    std::map<void*, bool> scheduled_for_current_time_eval;
    HistoryLog _log;
    PinHistoryLog _pin_log;
};
