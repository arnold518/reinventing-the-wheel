#pragma once

#include "simulator/SimulationTime.hpp"

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
#include "basic/WireBase.hpp"
#include "simulator/Event.hpp"
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <cstdint>
#include <utility>
#include <vector>

enum class DrainStatus {
    Idle,
    DeadlineReached,
    EventLimitReached,
};

struct DrainResult {
    DrainStatus status = DrainStatus::Idle;
    SimulationTime final_time = 0;
    size_t processed_events = 0;

    bool idle() const noexcept { return status == DrainStatus::Idle; }
};

struct SimulatorPerformanceCounters {
    uint64_t scheduled_events = 0;
    uint64_t scheduled_wire_updates = 0;
    uint64_t scheduled_component_evaluations = 0;
    uint64_t processed_events = 0;
    uint64_t processed_wire_updates = 0;
    uint64_t processed_component_evaluations = 0;
    uint64_t effective_wire_changes = 0;
    uint64_t effective_pin_changes = 0;
    size_t maximum_event_queue_depth = 0;
};

class Simulator {
public:
    using WireHistory = std::vector<std::pair<SimulationTime, std::vector<LogicValue>>>;
    using HistoryLog = std::map<std::shared_ptr<WireBase>, WireHistory>;
    using PinHistory = std::vector<std::pair<SimulationTime, std::vector<LogicValue>>>;
    using PinHistoryLog = std::map<std::shared_ptr<PinBase>, PinHistory>;

    Simulator();

    SimulationTime getCurrentTime() const;
    void scheduleEvent(std::shared_ptr<Event> event);

    void advanceAndRecord(SimulationTime target_time);
    void runAndRecord(SimulationTime max_time);
    DrainResult drainUntilIdle(SimulationTime deadline, size_t max_events);
    bool hasPendingEvents() const noexcept;
    std::optional<SimulationTime> nextEventTime() const;
    void setCircuitStateAtTime(SimulationTime target_time);
    std::vector<SimulationTime> getUniqueTimestamps() const;
    // Read-only observation. Inputs use their driving wire; primitive outputs
    // can have pin-only history. Neither method scrubs or advances the circuit.
    const PinHistory& signalHistory(const std::shared_ptr<PinBase>& pin) const;
    size_t historyRevision() const noexcept { return history_revision_; }
    void recordChange(SimulationTime time, std::shared_ptr<WireBase> wire, const std::vector<LogicValue>& value);
    void recordChange(SimulationTime time, std::shared_ptr<Wire<>> wire, LogicValue value);
    void recordPinChange(SimulationTime time, std::shared_ptr<PinBase> pin, const std::vector<LogicValue>& value);
    void setHistoryRecordingEnabled(bool enabled);
    bool isHistoryRecordingEnabled() const noexcept;
    const SimulatorPerformanceCounters& getPerformanceCounters() const noexcept;
    void resetPerformanceCounters() noexcept;
    void clear();

private:
    void recordProcessedEvent(const std::shared_ptr<Event>& event) noexcept;

    SimulationTime current_time;
    uint64_t next_scheduling_order = 0;
    // Mutable so that the const queries can drop withdrawn events first.
    mutable std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, Event::EventComparator> event_queue;
    void dropCancelled() const noexcept;
    HistoryLog _log;
    PinHistoryLog _pin_log;
    PinHistoryLog _headless_pin_state;
    bool history_recording_enabled_ = true;
    size_t history_revision_ = 0;
    SimulatorPerformanceCounters performance_counters_;
};
