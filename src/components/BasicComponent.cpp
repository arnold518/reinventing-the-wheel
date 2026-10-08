#include "components/BasicComponent.hpp"
#include "basic/PinBase.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace {
constexpr size_t NO_ARC = std::numeric_limits<size_t>::max();
}

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
    int control = -1;
    bool first = true, fresh = false;
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
    for (const auto& [name, pin] : getAllOutputPins()) timed->output_index.emplace(name, timed->output_index.size());
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
    for (const auto& [value, arcs] : delays.arcs_by_control) timed->arcs_by_control.emplace(value, matrix(arcs));
    timed->worst.assign(outputs, 0);
    for (size_t out = 0; out < outputs; ++out) for (const size_t ticks : timed->arcs[out])
        if (ticks != NO_ARC) timed->worst[out] = std::max(timed->worst[out], ticks);
    timed->pending.assign(outputs, 0);
    timed->last.resize(inputs);
    timed->changed.assign(inputs, true);
    timed->table = std::move(delays);
    timed_ = std::move(timed);
}

void BasicComponent::evaluateAt(size_t current_time, Simulator& simulator) {
    if (timed_) {
        for (size_t i = 0; i < timed_->inputs.size(); ++i) {
            auto now = timed_->inputs[i]->getValueAsVector();
            timed_->changed[i] = timed_->first || now != timed_->last[i];
            timed_->last[i] = std::move(now);
        }
        timed_->first = false;
        timed_->fresh = true;
    }
    evaluate(current_time, simulator);
    if (timed_) timed_->fresh = false;
}

size_t BasicComponent::publishTime(const std::string& output_pin, size_t current_sim_time) {
    if (!timed_) return current_sim_time + delay;
    auto& t = *timed_;
    const auto found = t.output_index.find(output_pin);
    if (found == t.output_index.end()) return current_sim_time + delay;
    const size_t out = found->second;
    const TimedOutputs::Matrix* arcs = &t.arcs;
    if (t.control >= 0 && !t.arcs_by_control.empty()) {
        // The value the control input has now selects its own set of delays.
        uint64_t value = 0;
        bool binary = true;
        const auto& bits = t.last[t.control];
        for (size_t bit = 0; bit < bits.size() && binary; ++bit) {
            if (bits[bit] == LogicValue::HIGH) value |= uint64_t{1} << bit;
            else binary = bits[bit] == LogicValue::LOW;
        }
        if (binary) if (const auto it = t.arcs_by_control.find(value); it != t.arcs_by_control.end()) arcs = &it->second;
    }
    size_t ticks = NO_ARC;
    if (t.fresh) for (size_t in = 0; in < t.changed.size(); ++in)
        if (t.changed[in] && (*arcs)[out][in] != NO_ARC) ticks = ticks == NO_ARC ? (*arcs)[out][in] : std::max(ticks, (*arcs)[out][in]);
    // Nothing listed explains a change of this output: stay with its worst case.
    if (ticks == NO_ARC) ticks = t.worst[out];
    // A newer result must not overtake an older one that is still on its way.
    t.pending[out] = std::max(current_sim_time + ticks, t.pending[out]);
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
