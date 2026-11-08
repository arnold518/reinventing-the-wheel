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
    // The recorded ticks after `after`, sorted: each history read from its
    // end, so a session that grows only pays for what is new.
    std::vector<SimulationTime> uniqueTimestampsAfter(SimulationTime after) const;
    // Read-only observation. Inputs use their driving wire; primitive outputs
    // can have pin-only history. Neither method scrubs or advances the circuit.
    const PinHistory& signalHistory(const std::shared_ptr<PinBase>& pin) const;
    size_t historyRevision() const noexcept { return history_revision_; }
    // True when a recorded wire change needs its value (history or a trace).
    bool changeValuesObserved() const;
    void countEffectiveWireChange() noexcept { ++performance_counters_.effective_wire_changes; }
    void recordChange(SimulationTime time, std::shared_ptr<WireBase> wire, const std::vector<LogicValue>& value);
    void recordChange(SimulationTime time, std::shared_ptr<Wire<>> wire, LogicValue value);
    void recordPinChange(SimulationTime time, std::shared_ptr<PinBase> pin, const std::vector<LogicValue>& value);
    void setHistoryRecordingEnabled(bool enabled);
    bool isHistoryRecordingEnabled() const noexcept;
    const SimulatorPerformanceCounters& getPerformanceCounters() const noexcept;
    void resetPerformanceCounters() noexcept;
    void clear();

    // Saving and restoring a machine (circuit::state::saveMachine).
    struct QueuedEventRecord {
        SimulationTime time;
        EventPriority priority;
        uint64_t order;
        std::shared_ptr<Event> event;
    };
    using HeadlessPinState = std::pair<std::shared_ptr<PinBase>, std::pair<SimulationTime, std::vector<LogicValue>>>;
    // The queued events in processing order, without withdrawn ones.
    std::vector<QueuedEventRecord> queuedEvents() const;
    uint64_t nextSchedulingOrder() const noexcept { return next_scheduling_order; }
    // With history off: the last value of each output pin that has no wire.
    std::vector<HeadlessPinState> headlessPinStates() const;
    // Replaces the time, queue, counters and headless pin values. Recorded
    // history is cleared; it restarts at `now`.
    void restoreScheduling(SimulationTime now, uint64_t next_order, std::vector<QueuedEventRecord> events,
                           const SimulatorPerformanceCounters& counters, std::vector<HeadlessPinState> headless);
    // Drops recorded history before `start`; each wire and pin keeps the value
    // it had then, now recorded at `start`. Bounds a long session's memory.
    void trimHistoryBefore(SimulationTime start);
    // Back at the present after viewing history (circuit::state::restoreValues).
    void restoreTime(SimulationTime now) noexcept { current_time = now; }
    // With history on: every wire and pin-only output starts its history at
    // its current (restored) value.
    void seedHistory(SimulationTime now, const std::vector<std::shared_ptr<WireBase>>& wires,
                     const std::vector<HeadlessPinState>& pins);

private:
    void recordProcessedEvent(const std::shared_ptr<Event>& event) noexcept;

    SimulationTime current_time;
    uint64_t next_scheduling_order = 0;
    // The queue keeps each event's sort keys beside it, so ordering compares
    // plain values instead of dereferencing events and calling getPriority():
    // time, then wire updates before evaluations, then scheduling order, as
    // Event::EventComparator.
    struct QueuedEvent {
        SimulationTime time;
        EventPriority priority;
        uint64_t order;
        std::shared_ptr<Event> event;
    };
    struct QueuedEventLater {
        bool operator()(const QueuedEvent& a, const QueuedEvent& b) const {
            if (a.time != b.time) return a.time > b.time;
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.order > b.order;
        }
    };
    // Mutable so that the const queries can drop withdrawn events first.
    mutable std::priority_queue<QueuedEvent, std::vector<QueuedEvent>, QueuedEventLater> event_queue;
    void dropCancelled() const noexcept;
    HistoryLog _log;
    PinHistoryLog _pin_log;
    PinHistoryLog _headless_pin_state;
    bool history_recording_enabled_ = true;
    size_t history_revision_ = 0;
    SimulatorPerformanceCounters performance_counters_;
};
