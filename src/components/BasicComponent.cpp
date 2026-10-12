#include "components/BasicComponent.hpp"
#include "basic/PinBase.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace {
constexpr size_t NO_ARC = std::numeric_limits<size_t>::max();
size_t setup_violations = 0;
}

size_t BasicComponent::setupViolations() { return setup_violations; }
void BasicComponent::resetSetupViolations() { setup_violations = 0; }

// The delay table, prepared for use while simulating.
struct BasicComponent::TimedOutputs {
    // [output][input] -> ticks, or NO_ARC.
    using Matrix = std::vector<std::vector<size_t>>;
    circuit::PartDelays table;
    std::vector<std::shared_ptr<PinBase>> inputs;
    std::vector<std::vector<LogicValue>> last;
    std::vector<bool> changed;
    std::unordered_map<std::string, size_t> output_index;
    Matrix arcs;
    std::map<uint64_t, Matrix> arcs_by_control;
    std::vector<size_t> worst, pending;
    // Setup watching: the clock input, the watched inputs with their setup
    // ticks, the inputs that must sit at a fixed value (reset low), and when
    // each input last changed.
    int clock = -1;
    std::vector<std::pair<size_t, size_t>> watched;
    std::vector<std::pair<size_t, uint64_t>> fixed;
    std::vector<size_t> changed_at;
    int control = -1;
    bool first = true, fresh = false;
    std::vector<bool> explained;
    // With a delay twin: when each input bit last changed, and the latest
    // publish time handed out per output (results stay in order).
    std::shared_ptr<const circuit::DelayTwin> twin;
    std::unique_ptr<circuit::DelayTwin::State> twin_state;
    std::vector<std::vector<uint8_t>> twin_value;
    bool twin_answered = false;
    std::vector<std::vector<long long>> bit_changed;
    std::vector<long long> settled;
    std::vector<size_t> released, before;
    // Strict settling (CIRCUITSIM_STRICT_SETTLING=1), for parts with a twin:
    // an output that may still move shows X from the earliest moment it can
    // until it is final, also when it ends on the value it had. The gates can
    // pulse there; the compact part then no longer looks settled before them.
    bool strict = false;
    Matrix soonest;                       // [output][input] -> earliest ticks, or NO_ARC
    std::vector<bool> disturbed, published_now;
    std::vector<size_t> window_start, released_before;
    // Each output pin and the value last handed out for it.
    std::vector<std::shared_ptr<PinBase>> outputs;
    std::vector<std::vector<LogicValue>> published;
    // The delays that apply under the value the control input has now.
    const Matrix& arcsNow() const {
        if (control < 0 || arcs_by_control.empty()) return arcs;
        uint64_t value = 0;
        const auto& bits = last[control];
        for (size_t bit = 0; bit < bits.size(); ++bit) {
            if (bits[bit] == LogicValue::HIGH) value |= uint64_t{1} << bit;
            else if (bits[bit] != LogicValue::LOW) return arcs;
        }
        const auto found = arcs_by_control.find(value);
        return found == arcs_by_control.end() ? arcs : found->second;
    }
};

BasicComponent::BasicComponent(std::string name, size_t delay_val, PinInitFunction initializer)
    : IOComponent(std::move(name), std::move(initializer)), delay(delay_val) {}

BasicComponent::~BasicComponent() = default;

size_t BasicComponent::getDelay() const {
    return delay;
}

const circuit::PartDelays* BasicComponent::getDelays() const {
    return timed_ ? &timed_->table : nullptr;
}

void BasicComponent::setDelays(circuit::PartDelays delays) {
    if (delays.empty()) { timed_.reset(); return; }
    auto timed = std::make_unique<TimedOutputs>();
    std::unordered_map<std::string, size_t> input_index;
    for (const auto& [name, pin] : getAllInputPins()) {
        input_index.emplace(name, timed->inputs.size());
        if (name == delays.control) timed->control = static_cast<int>(timed->inputs.size());
        timed->inputs.push_back(pin);
    }
    for (const auto& [name, pin] : getAllOutputPins()) {
        timed->output_index.emplace(name, timed->output_index.size());
        timed->outputs.push_back(pin);
        timed->published.push_back(pin->getValueAsVector());
    }
    const size_t outputs = timed->output_index.size(), inputs = timed->inputs.size();
    const auto matrix = [&](const std::vector<circuit::PartDelays::Arc>& arcs) {
        TimedOutputs::Matrix result(outputs, std::vector<size_t>(inputs, NO_ARC));
        const auto set = [&](const std::string& input, const std::string& output, size_t ticks) {
            const auto in = input_index.find(input);
            const auto out = timed->output_index.find(output);
            if (in == input_index.end() || out == timed->output_index.end())
                throw std::invalid_argument(getID() + ": delay table names a missing pin " + input + " -> " + output);
            auto& cell = result[out->second][in->second];
            cell = cell == NO_ARC ? ticks : std::max(cell, ticks);
        };
        for (const auto& arc : arcs) set(arc.input, arc.output, arc.delay.maximum);
        // Clocked outputs follow the clock pin.
        for (const auto& [output, range] : delays.launches) set(delays.clock, output, range.maximum);
        return result;
    };
    timed->arcs = matrix(delays.arcs);
    timed->soonest.assign(outputs, std::vector<size_t>(inputs, NO_ARC));
    for (const auto& arc : delays.arcs) {
        const auto in = input_index.find(arc.input);
        const auto out = timed->output_index.find(arc.output);
        if (in == input_index.end() || out == timed->output_index.end()) continue;
        auto& cell = timed->soonest[out->second][in->second];
        cell = std::min(cell, arc.delay.minimum);
    }
    // A clocked output moves no sooner than its earliest launch after the clock.
    if (const auto clock = input_index.find(delays.clock); clock != input_index.end())
        for (const auto& [output, range] : delays.launches)
            if (const auto out = timed->output_index.find(output); out != timed->output_index.end()) {
                auto& cell = timed->soonest[out->second][clock->second];
                cell = std::min(cell, range.minimum);
            }
    for (const auto& [value, arcs] : delays.arcs_by_control) timed->arcs_by_control.emplace(value, matrix(arcs));
    timed->worst.assign(outputs, 0);
    for (size_t out = 0; out < outputs; ++out) for (const size_t ticks : timed->arcs[out])
        if (ticks != NO_ARC) timed->worst[out] = std::max(timed->worst[out], ticks);
    timed->pending.assign(outputs, 0);
    timed->explained.assign(outputs, false);
    if (const auto found = input_index.find(delays.clock); found != input_index.end()) timed->clock = static_cast<int>(found->second);
    for (const auto& [input, ticks] : delays.setup)
        if (const auto found = input_index.find(input); found != input_index.end() && ticks) timed->watched.emplace_back(found->second, ticks);
    for (const auto& [input, value] : delays.fixed_inputs)
        if (const auto found = input_index.find(input); found != input_index.end()) timed->fixed.emplace_back(found->second, value);
    timed->changed_at.assign(inputs, 0);
    timed->last.resize(inputs);
    timed->changed.assign(inputs, true);
    // The twin is used only if it describes exactly these pins.
    if (delays.twin && (delays.twin->inputs.size() != inputs || delays.twin->outputs.size() != outputs) && std::getenv("CIRCUITSIM_DELAY_TRACE"))
        std::cerr << "delay twin of " << getID() << " has " << delays.twin->inputs.size() << " inputs and " << delays.twin->outputs.size()
                  << " outputs, the part " << inputs << " and " << outputs << '\n';
    if (delays.twin && delays.twin->inputs.size() == inputs && delays.twin->outputs.size() == outputs) {
        bool same = true;
        for (size_t i = 0; i < inputs; ++i)
            same = same && delays.twin->inputs[i] == std::pair<std::string, size_t>(timed->inputs[i]->getName(), timed->inputs[i]->getWidth());
        for (size_t o = 0; o < outputs; ++o)
            same = same && delays.twin->outputs[o] == std::pair<std::string, size_t>(timed->outputs[o]->getName(), timed->outputs[o]->getWidth());
        if (!same && std::getenv("CIRCUITSIM_DELAY_TRACE")) {
            std::cerr << "delay twin of " << getID() << " has other pins:";
            for (size_t i = 0; i < inputs; ++i) if (delays.twin->inputs[i] != std::pair<std::string, size_t>(timed->inputs[i]->getName(), timed->inputs[i]->getWidth()))
                std::cerr << ' ' << delays.twin->inputs[i].first << '/' << delays.twin->inputs[i].second << " vs " << timed->inputs[i]->getName() << '/' << timed->inputs[i]->getWidth();
            for (size_t o = 0; o < outputs; ++o) if (delays.twin->outputs[o] != std::pair<std::string, size_t>(timed->outputs[o]->getName(), timed->outputs[o]->getWidth()))
                std::cerr << ' ' << delays.twin->outputs[o].first << '/' << delays.twin->outputs[o].second << " vs " << timed->outputs[o]->getName() << '/' << timed->outputs[o]->getWidth();
            std::cerr << '\n';
        }
        if (same) {
            timed->twin = delays.twin;
            timed->twin_state = delays.twin->start();
            for (size_t i = 0; i < inputs; ++i) timed->bit_changed.emplace_back(timed->inputs[i]->getWidth(), -1);
        }
    }
    timed->released.assign(outputs, 0);
    timed->strict = timed->twin && std::getenv("CIRCUITSIM_STRICT_SETTLING");
    timed->disturbed.assign(outputs, false);
    timed->published_now.assign(outputs, false);
    timed->window_start.assign(outputs, 0);
    timed->table = std::move(delays);
    timed_ = std::move(timed);
}

void BasicComponent::evaluateAt(size_t current_time, Simulator& simulator) {
    if (timed_) {
        const bool started = !timed_->first;
        const bool was_low = timed_->clock >= 0 && started && timed_->last[timed_->clock] == std::vector<LogicValue>{LogicValue::LOW};
        for (size_t i = 0; i < timed_->inputs.size(); ++i) {
            auto now = timed_->inputs[i]->getValueAsVector();
            timed_->changed[i] = timed_->first || now != timed_->last[i];
            if (timed_->twin && timed_->changed[i]) for (size_t bit = 0; bit < now.size(); ++bit)
                if (timed_->first || now[bit] != timed_->last[i][bit]) timed_->bit_changed[i][bit] = static_cast<long long>(current_time);
            if (timed_->changed[i] && started && static_cast<int>(i) != timed_->clock) timed_->changed_at[i] = current_time + 1;
            timed_->last[i] = std::move(now);
        }
        // A rising clock edge: was every watched input quiet for its setup time?
        if (was_low && timed_->last[timed_->clock] == std::vector<LogicValue>{LogicValue::HIGH}) {
            bool operating = true;
            for (const auto& [input, value] : timed_->fixed) {
                const auto& bits = timed_->last[input];
                for (size_t bit = 0; bit < bits.size(); ++bit)
                    operating = operating && bits[bit] == (((value >> bit) & 1) ? LogicValue::HIGH : LogicValue::LOW);
            }
            if (operating) for (const auto& [input, ticks] : timed_->watched)
                // changed_at holds the tick after the change; 0 means never.
                if (timed_->changed_at[input] && current_time + 1 - timed_->changed_at[input] < ticks) {
                    if (!setup_violations && std::getenv("CIRCUITSIM_SETUP_TRACE"))
                        std::cerr << "setup: " << getID() << " input " << timed_->inputs[input]->getName() << " changed "
                                  << current_time + 1 - timed_->changed_at[input] << " ticks before the edge at " << current_time
                                  << ", needs " << ticks << '\n';
                    ++setup_violations;
                }
        }
        // Every input that changed keeps the outputs it reaches busy for its
        // delay, whether or not the value computed now differs: a later, faster
        // input must not publish a result the slower one is still shaping.
        const auto& arcs = timed_->arcsNow();
        const bool control_changed = timed_->control >= 0 && timed_->changed[timed_->control];
        // Clocked outputs are launched by the rising edge only; a falling
        // edge starts nothing, so it holds nothing back.
        const int idle_clock = timed_->clock >= 0 && timed_->changed[timed_->clock] && started
            && timed_->last[timed_->clock] != std::vector<LogicValue>{LogicValue::HIGH} ? timed_->clock : -1;
        for (size_t out = 0; out < arcs.size(); ++out) {
            size_t ticks = NO_ARC;
            // A new control value selects other delays for inputs that may
            // still be on their way; only the worst case covers them all.
            if (control_changed) ticks = timed_->worst[out];
            else for (size_t in = 0; in < timed_->changed.size(); ++in)
                if (timed_->changed[in] && static_cast<int>(in) != idle_clock && arcs[out][in] != NO_ARC) ticks = ticks == NO_ARC ? arcs[out][in] : std::max(ticks, arcs[out][in]);
            timed_->explained[out] = ticks != NO_ARC;
            if (ticks != NO_ARC) timed_->pending[out] = std::max(timed_->pending[out], current_time + ticks);
        }
        // The gates, timed with the values the inputs have now. Each output
        // takes that time when it is published, if the gates end on the
        // value the part computed.
        if (timed_->twin)
            timed_->twin_answered = timed_->twin->settle(timed_->twin_state.get(), static_cast<long long>(current_time), timed_->last,
                                                         timed_->bit_changed, timed_->settled, timed_->twin_value);
        if (timed_->strict && timed_->twin_answered) {
            timed_->released_before = timed_->released;
            for (size_t out = 0; out < timed_->pending.size(); ++out) {
                timed_->disturbed[out] = timed_->published_now[out] = false;
                if (timed_->settled[out] <= static_cast<long long>(current_time)) continue;
                size_t start = NO_ARC;
                for (size_t in = 0; in < timed_->changed.size(); ++in)
                    if (timed_->changed[in] && timed_->soonest[out][in] != NO_ARC) start = std::min(start, timed_->soonest[out][in]);
                if (start == NO_ARC) continue;
                // Final when the gates are, whatever the table says.
                timed_->pending[out] = std::max(static_cast<size_t>(timed_->settled[out]), timed_->released[out]);
                timed_->window_start[out] = current_time + start;
                timed_->disturbed[out] = timed_->window_start[out] < timed_->pending[out];
            }
        }
        timed_->first = false;
        timed_->fresh = true;
    }
    evaluate(current_time, simulator);
    if (timed_) timed_->fresh = false;
    if (timed_ && timed_->strict && timed_->twin_answered) for (size_t out = 0; out < timed_->pending.size(); ++out) {
        if (!timed_->disturbed[out]) continue;
        const auto& pin = timed_->outputs[out];
        const auto wire = pin->getExternalWireBase();
        if (!wire) continue;
        const std::vector<LogicValue> unknown(pin->getWidth(), LogicValue::UNKNOWN);
        const size_t final_at = timed_->pending[out];
        if (auto event = makeWireUpdateEvent(timed_->window_start[out], wire, unknown)) simulator.scheduleEvent(std::move(event));
        // A result of an earlier change still due inside the window is covered again.
        const size_t earlier = timed_->released_before[out];
        if (earlier > timed_->window_start[out] && earlier < final_at)
            if (auto event = makeWireUpdateEvent(earlier, wire, unknown)) simulator.scheduleEvent(std::move(event));
        // The part published nothing new: the value it has returns when the gates are final.
        if (!timed_->published_now[out]) {
            if (auto event = makeWireUpdateEvent(final_at, wire, pin->getValueAsVector())) simulator.scheduleEvent(std::move(event));
            timed_->released[out] = final_at;
        }
    }
}

size_t BasicComponent::publishTime(const std::string& output_pin, size_t current_sim_time) {
    if (!timed_) return current_sim_time + delay;
    auto& t = *timed_;
    const auto found = t.output_index.find(output_pin);
    if (found == t.output_index.end()) return current_sim_time + delay;
    const size_t out = found->second;
    // The pin already holds the value being published. An output merely
    // restated at the value it has is no change.
    auto value = t.outputs[out]->getValueAsVector();
    const bool value_changes = value != t.published[out];
    // Never later than the table, earlier wherever the data cuts a path short.
    bool by_gates = t.twin && t.fresh && t.twin_answered && t.settled[out] >= 0 && t.twin_value[out].size() == value.size();
    for (size_t bit = 0; by_gates && bit < value.size(); ++bit) {
        const uint8_t level = value[bit] == LogicValue::LOW ? 0 : value[bit] == LogicValue::HIGH ? 1 : 2;
        // 3: a bit the gate list times without knowing its value.
        by_gates = t.twin_value[out][bit] == 3 || level == t.twin_value[out][bit];
    }
    if (t.twin && t.fresh && !by_gates && value_changes && std::getenv("CIRCUITSIM_DELAY_TRACE"))
        std::cerr << "gate list not used: " << getID() << " output " << output_pin << " at " << current_sim_time
                  << (!t.twin_answered ? ": no answer" : t.settled[out] < 0 ? ": never changes by its gates" : ": another value") << '\n';
    t.published[out] = std::move(value);
    if (t.fresh && !t.explained[out] && value_changes && std::getenv("CIRCUITSIM_DELAY_TRACE")) {
        std::cerr << "unexplained: " << getID() << " output " << output_pin << " at " << current_sim_time << " after";
        for (size_t in = 0; in < t.changed.size(); ++in) if (t.changed[in]) std::cerr << ' ' << t.inputs[in]->getName();
        std::cerr << '\n';
    }
    // Nothing listed explains a change of this output: stay with its worst case.
    if (!t.fresh || (!t.explained[out] && value_changes)) t.pending[out] = std::max(current_sim_time + t.worst[out], t.pending[out]);
    if (by_gates && !t.strict)
        t.pending[out] = std::max({std::min(t.pending[out], static_cast<size_t>(t.settled[out])), current_sim_time, t.released[out]});
    // A newer result never overtakes an older one that is still on its way.
    t.released[out] = t.pending[out];
    if (t.fresh) t.published_now[out] = true;
    return t.pending[out];
}

void BasicComponent::scheduleInitialEvents(Simulator& simulator,
                                           size_t current_time) {
    // Most evaluated components are triggered by their input-wire updates.
    // Self-starting components such as ConstantValue override this hook, while
    // ClockGenerator is started explicitly so it cannot be scheduled twice.
    (void)simulator;
    (void)current_time;
}

void BasicComponent::_updateOutputWire(Simulator& simulator,
                                       const std::string& pin_name,
    LogicValue new_value,
    size_t current_sim_time) {
    if (auto pin = getOutputPin(pin_name)) {
        pin->setBit(0, new_value);
        if (auto wire = pin->getExternalWire()) {
            simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(
                publishTime(pin_name, current_sim_time), wire, new_value));
        } else {
            simulator.recordPinChange(
                publishTime(pin_name, current_sim_time), pin, pin->getValueAsVector());
        }
    }
}

void BasicComponent::_updateOutputWireDynamic(Simulator& simulator,
                                              const std::string& pin_name,
    const std::vector<LogicValue>& new_value,
    size_t current_sim_time) {
    const auto pin = getOutputPinDynamic(pin_name);
    if (!pin || pin->getWidth() != new_value.size()) {
        return;
    }
    pin->setValueFromVector(new_value);
    if (auto wire = pin->getExternalWireBase()) {
        auto event = makeWireUpdateEvent(publishTime(pin_name, current_sim_time), wire, new_value);
        if (!event) {
            throw std::logic_error("Unsupported output pin width");
        }
        simulator.scheduleEvent(std::move(event));
    } else {
        simulator.recordPinChange(
            publishTime(pin_name, current_sim_time), pin, pin->getValueAsVector());
    }
}
