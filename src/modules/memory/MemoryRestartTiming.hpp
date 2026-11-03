#pragma once

#include "simulator/SimulationTime.hpp"
#include "components/BasicComponent.hpp"
#include "components/PartDelays.hpp"
#include "simulator/Event.hpp"
#include <algorithm>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace circuit::memory_detail {

// Output timing of a compact self-timed device, read from the table proven on
// its gate form: an input change reaches an output no sooner than the arc's
// minimum (the earliest effect, for example a completion signal dropping)
// and has settled at the arc's maximum (for example a completion signal
// rising after the whole access). Without a table, `fallback` is used.
struct ArcBounds { SimulationTime minimum = 0, maximum = 0; };
// `control`: the value on the table's control pin, if it is binary; the
// arcs proven for that value are used then.
inline ArcBounds arcBounds(const circuit::PartDelays* table, const std::vector<std::string>& inputs,
                           const std::string& output, ArcBounds fallback, std::optional<uint64_t> control = std::nullopt) {
    if (!table) return fallback;
    bool found = false;
    ArcBounds bounds{MAX_SIMULATION_TIME, 0};
    const auto* arcs = &table->arcs;
    if (control && !table->control.empty())
        if (const auto chosen = table->arcs_by_control.find(*control); chosen != table->arcs_by_control.end()) arcs = &chosen->second;
    for (const auto& arc : *arcs) {
        if (arc.output != output || std::find(inputs.begin(), inputs.end(), arc.input) == inputs.end()) continue;
        found = true;
        bounds.minimum = std::min(bounds.minimum, arc.delay.minimum);
        bounds.maximum = std::max(bounds.maximum, arc.delay.maximum);
    }
    return found ? bounds : fallback;
}

// The earliest a change of several pins can show, never sooner than its
// gates: a pin's quickest path may be closed by another input (a write mask
// during a read), so the latest of the pins' minimum delays is taken.
inline SimulationTime safeEarliest(const circuit::PartDelays* table, const std::vector<std::string>& inputs,
                           const std::string& output, SimulationTime fallback, std::optional<uint64_t> control = std::nullopt) {
    SimulationTime earliest = 0;
    for (const auto& input : inputs)
        earliest = std::max(earliest, arcBounds(table, {input}, output, {fallback, fallback}, control).minimum);
    return inputs.empty() ? fallback : earliest;
}

// Scheduled changes of one output wire, withdrawn when superseded: like an
// inertial gate, a newer change cancels every pending one first.
template<size_t W>
class PendingOutput {
public:
    void cancel() {
        for (auto& event : pending_) if (auto e = event.lock()) e->cancelled = true;
        pending_.clear();
    }
    // Like BasicComponent's own outputs, the pin holds the latest value
    // computed; its wire changes at `time`.
    void schedule(Simulator& sim, const std::shared_ptr<Pin<W>>& pin, SimulationTime time, const std::vector<LogicValue>& value) {
        if (!pin) return;
        pin->setValueFromVector(value);
        const auto wire = pin->getExternalWire();
        if (!wire) { sim.recordPinChange(time, pin, value); return; }
        std::shared_ptr<Event> event = std::make_shared<WireUpdateEvent>(time, wire, value);
        pending_.push_back(event);
        sim.scheduleEvent(std::move(event));
    }
private:
    std::vector<std::weak_ptr<Event>> pending_;
};

// One output of a compact self-timed part as a list of future changes. Each
// change is applied at its own time: the part wakes for it and only then
// sets the pin and the wire, so no value appears early (BasicComponent sets a
// pin when it computes it, which for a long access would be far too soon).
// clear() withdraws every future change, as an inertial gate does.
template<size_t W>
class TimedOutput {
public:
    explicit TimedOutput(std::vector<LogicValue> initial) : current_(std::move(initial)) {}
    void clear() { future_.clear(); }
    void at(SimulationTime time, std::vector<LogicValue> value) { future_.push_back({time, std::move(value)}); }
    // The earliest still-pending time at which the output takes `value`.
    std::optional<SimulationTime> pendingAt(const std::vector<LogicValue>& value) const {
        std::optional<SimulationTime> earliest;
        for (const auto& [time, v] : future_) if (v == value && (!earliest || time < *earliest)) earliest = time;
        return earliest;
    }
    std::optional<SimulationTime> next() const {
        std::optional<SimulationTime> earliest;
        for (const auto& [time, value] : future_) if (!earliest || time < *earliest) earliest = time;
        return earliest;
    }
    // Applies every change due by `t` (in order of time) and shows the result.
    void apply(Simulator& sim, const std::shared_ptr<Pin<W>>& pin, SimulationTime t) {
        std::stable_sort(future_.begin(), future_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        auto value = current_;
        size_t taken = 0;
        while (taken < future_.size() && future_[taken].first <= t) value = future_[taken++].second;
        future_.erase(future_.begin(), future_.begin() + taken);
        if (value == current_ && published_) return;
        current_ = value; published_ = true;
        if (!pin) return;
        pin->setValueFromVector(value);
        if (const auto wire = pin->getExternalWire()) sim.scheduleEvent(std::make_shared<WireUpdateEvent>(t, wire, value));
        else sim.recordPinChange(t, pin, value);
    }
    const std::vector<LogicValue>& value() const { return current_; }
private:
    std::vector<std::pair<SimulationTime, std::vector<LogicValue>>> future_;
    std::vector<LogicValue> current_;
    bool published_ = false;
};

} // namespace circuit::memory_detail
