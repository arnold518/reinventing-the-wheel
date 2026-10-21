#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include <algorithm>
#include "modules/basic/Gate.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include <stdexcept>
#include "components/TimingProfile.hpp"
using PinPtr = circuit::detail::StructuralWiring::PinPtr;

namespace circuit::memory {

MemoryDelayLine::MemoryDelayLine(std::string name, SimulationTime delay, SimulationTime fall_delay)
    : BasicComponent(std::move(name), delay, [](IOComponent* self) {
          self->addPin("IN", PinType::INPUT);
          self->addPin("OUT", PinType::OUTPUT);
      }), fall_delay_(std::min(fall_delay, delay)) {}

// Inertial: a new input withdraws every change still pending, so a pulse
// shorter than the delay does not pass and an edge never arrives out of order.
void MemoryDelayLine::evaluate(SimulationTime time, Simulator& simulator) {
    auto in = getInputValue("IN");
    if (in == LogicValue::HIGH_Z) in = LogicValue::UNKNOWN;
    for (auto& event : pending_) if (auto e = event.lock()) e->cancelled = true;
    pending_.clear();
    auto pin = getOutputPin("OUT");
    pin->setBit(0, in);
    const auto wire = pin->getExternalWire();
    if (!wire) return;
    if (wire->getValueVector()[0] == in) return;
    const SimulationTime at = checkedTimeAdd(time, in == LogicValue::HIGH ? delay : fall_delay_);
    std::shared_ptr<Event> event = std::make_shared<WireUpdateEvent<>>(at, wire, in);
    pending_.push_back(event);
    simulator.scheduleEvent(std::move(event));
}

SymbolicDiffer::SymbolicDiffer(std::string name)
    : BasicComponent(std::move(name), circuit::timing::leafDelay(circuit::timing::Leaf::Xor), [](IOComponent* self) {
          self->addPin("A", PinType::INPUT);
          self->addPin("B", PinType::INPUT);
          self->addPin("OUT", PinType::OUTPUT);
      }) {}

void SymbolicDiffer::evaluate(SimulationTime time, Simulator& simulator) {
    _updateOutputWire(simulator, "OUT", getInputValue("A") == getInputValue("B") ? LogicValue::LOW : LogicValue::HIGH, time);
}

ProtocolStableData::ProtocolStableData(std::string name, size_t width)
    : BasicComponent(std::move(name), 0, [width](IOComponent* self) {
          self->addPinDynamic("IN", PinType::INPUT, width);
          self->addPinDynamic("OUT", PinType::OUTPUT, width);
      }) {}

void ProtocolStableData::evaluate(SimulationTime time, Simulator& simulator) {
    _updateOutputWireDynamic(simulator, "OUT", getInputPinDynamic("IN")->getValueAsVector(), time);
}

KnownBits::KnownBits(std::string name, size_t width)
    : BasicComponent(std::move(name), circuit::timing::leafDelay(circuit::timing::Leaf::MemoryPrimitive), [width](IOComponent* self) {
          self->addPinDynamic("IN", PinType::INPUT, width);
          self->addPin("KNOWN", PinType::OUTPUT);
      }) {}

void KnownBits::evaluate(SimulationTime time, Simulator& simulator) {
    bool known = true;
    for (auto bit : getInputPinDynamic("IN")->getValueAsVector()) known = known && (bit == LogicValue::LOW || bit == LogicValue::HIGH);
    _updateOutputWire(simulator, "KNOWN", known ? LogicValue::HIGH : LogicValue::LOW, time);
}

PowerOnReset::PowerOnReset(std::string name, SimulationTime width)
    : BasicComponent(std::move(name), 0, [](IOComponent* self) { self->addPin("OUT", PinType::OUTPUT); }), width_(width) {}

void PowerOnReset::scheduleInitialEvents(Simulator& simulator, SimulationTime time) {
    auto pin = getOutputPin("OUT");
    pin->setBit(0, LogicValue::HIGH);
    if (auto wire = pin->getExternalWire()) {
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time, wire, LogicValue::HIGH));
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(checkedTimeAdd(time, width_), wire, LogicValue::LOW));
    }
    pin->setBit(0, LogicValue::LOW);
}

void PowerOnReset::evaluate(SimulationTime, Simulator&) {}

Synchronizer::Synchronizer(std::string name, size_t stages)
    : BasicComponent(std::move(name), circuit::dflipflop_timing::clockToOutputMaximum(), [](IOComponent* self) {
          for (const auto* pin : {"CLK", "RST", "D"}) self->addPin(pin, PinType::INPUT);
          self->addPin("Q", PinType::OUTPUT);
      }), stages_(stages) {
    if (stages != 1 && stages != 2) throw std::invalid_argument("A synchronizer has one or two stages");
}

void Synchronizer::evaluate(SimulationTime time, Simulator& simulator) {
    const auto clock = getInputValue("CLK");
    const auto reset = getInputValue("RST");
    if (reset == LogicValue::HIGH) {
        first_ = second_ = LogicValue::LOW;
    } else if (reset == LogicValue::LOW && previous_clock_ == LogicValue::LOW && clock == LogicValue::HIGH) {
        second_ = first_;
        const auto d = getInputValue("D");
        if (d == LogicValue::LOW || d == LogicValue::HIGH) first_ = d;
        if (stages_ == 1) second_ = first_;
    } else if (reset != LogicValue::LOW) {
        first_ = second_ = LogicValue::UNKNOWN;
    }
    previous_clock_ = clock;
    _updateOutputWire(simulator, "Q", second_, time);
}

MemoryWire::MemoryWire(std::string name, size_t width, SimulationTime delay)
    : BasicComponent(std::move(name), delay, [width](IOComponent* self) {
          if (!width || width > 32) throw std::invalid_argument("A memory wire carries 1 to 32 bits");
          self->addPinDynamic("IN", PinType::INPUT, width);
          self->addPinDynamic("OUT", PinType::OUTPUT, width);
      }),
      type_("MemoryWire<" + std::to_string(width) + ">") {
    if (!delay) throw std::invalid_argument("A memory wire needs a positive delay");
}

void MemoryWire::evaluate(SimulationTime time, Simulator& simulator) {
    _updateOutputWireDynamic(simulator, "OUT", getInputPinDynamic("IN")->getValueAsVector(), time);
}

MemoryBus::MemoryBus(std::string name, size_t drivers, size_t width, SimulationTime delay, bool pulled_low)
    : BasicComponent(std::move(name), delay, [drivers, width](IOComponent* self) {
          if (!drivers || !width || width > 32) throw std::invalid_argument("A memory bus needs drivers and 1 to 32 bits");
          for (size_t i = 0; i < drivers; ++i) self->addPinDynamic("IN_" + std::to_string(i), PinType::INPUT, width);
          self->addPinDynamic("OUT", PinType::OUTPUT, width);
      }),
      drivers_(drivers), width_(width), pulled_low_(pulled_low),
      type_("MemoryBus<" + std::to_string(drivers) + "," + std::to_string(width) + (pulled_low ? ",pulled-low" : "") + ">") {
    if (!delay) throw std::invalid_argument("A memory bus needs a positive delay");
}

void MemoryBus::evaluate(SimulationTime time, Simulator& simulator) {
    std::vector<LogicValue> out(width_, LogicValue::HIGH_Z);
    for (size_t i = 0; i < drivers_; ++i) {
        const auto value = getInputPinDynamic("IN_" + std::to_string(i))->getValueAsVector();
        for (size_t bit = 0; bit < width_; ++bit) {
            if (value[bit] == LogicValue::HIGH_Z) continue;
            out[bit] = out[bit] == LogicValue::HIGH_Z || out[bit] == value[bit] ? value[bit] : LogicValue::UNKNOWN;
        }
    }
    if (pulled_low_) for (auto& bit : out) if (bit == LogicValue::HIGH_Z) bit = LogicValue::LOW;
    _updateOutputWireDynamic(simulator, "OUT", out, time);
}

SimulationTime memoryTimerSettle(SimulationTime configured, size_t inputs, bool conditional) {
    using circuit::timing::Leaf;
    using circuit::timing::leafDelay;
    size_t levels = 0; while ((size_t{1} << levels) < inputs) ++levels;
    const SimulationTime detection = leafDelay(Leaf::Xor) + (conditional ? leafDelay(Leaf::And) : 0) + levels * leafDelay(Leaf::Or)
        + leafDelay(Leaf::Not) + 3 * leafDelay(Leaf::And);
    return std::max(configured, detection);
}

MemoryTimer::MemoryTimer(std::string name, size_t inputs, SimulationTime settle_ticks, std::vector<MemoryTimerStage> stages,
                         size_t conditional_from, size_t detected)
    : IOComponent(std::move(name), [inputs, count = stages.size(), conditional_from](IOComponent* self) {
          if (!inputs || !count) throw std::invalid_argument("A memory timer needs inputs and stages");
          self->addPin("ENABLE", PinType::INPUT);
          if (conditional_from < inputs) self->addPin("CONDITION", PinType::INPUT);
          for (size_t i = 0; i < inputs; ++i) {
              self->addPin("IN_" + std::to_string(i), PinType::INPUT);
              self->addPin("HELD_" + std::to_string(i), PinType::OUTPUT);
          }
          for (size_t k = 0; k <= count; ++k) self->addPin("STAGE_" + std::to_string(k), PinType::OUTPUT);
      }),
      inputs_(inputs), settle_(memoryTimerSettle(settle_ticks, std::min(inputs, detected), conditional_from < std::min(inputs, detected))),
      conditional_from_(conditional_from), detected_(std::min(inputs, detected)), stages_(std::move(stages)) {
    if (!settle_ticks) throw std::invalid_argument("A memory timer needs a positive settle delay");
    if (!detected_) throw std::invalid_argument("A memory timer must watch at least one input");
    for (const auto& stage : stages_)
        if (!stage.replica_gates) throw std::invalid_argument("Every timer stage needs at least one gate");
    type_ = "MemoryTimer<" + std::to_string(inputs_) + "," + std::to_string(stages_.size()) +
            (detected_ < inputs_ ? "," + std::to_string(detected_) : std::string()) + ">";
}

void MemoryTimer::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    std::vector<circuit::detail::StructuralWiring::PinPtr> changes;
    for (size_t i = 0; i < inputs_; ++i) {
        const auto n = std::to_string(i);
        auto held = b.addNewComponent<MemoryDelayLine>("SETTLE_" + n, settle_);
        w.connect(getInputPin("IN_" + n), held->getInputPin("IN"));
        w.connect(held->getOutputPin("OUT"), getOutputPin("HELD_" + n));
        if (i >= detected_) continue;
        auto differ = b.addNewComponent<SymbolicDiffer>("CHANGED_" + n);
        w.connect(getInputPin("IN_" + n), differ->getInputPin("A"));
        w.connect(held->getOutputPin("OUT"), differ->getInputPin("B"));
        PinPtr changed = differ->getOutputPin("OUT");
        if (i >= conditional_from_) changed = w.both("COUNTED_" + n, changed, getInputPin("CONDITION"));
        changes.push_back(changed);
    }
    auto changing = w.reduce("CHANGING", changes, false);
    auto start = w.both("STARTED", getInputPin("ENABLE"), w.invert("STEADY", changing));
    w.connect(start, getOutputPin("STAGE_0"));
    auto previous = start;
    for (size_t k = 0; k < stages_.size(); ++k) {
        const auto n = std::to_string(k + 1);
        auto signal = previous;
        if (stages_[k].delay_ticks) {
            // A stage resets as fast as one gate.
            auto line = b.addNewComponent<MemoryDelayLine>("INTERVAL_" + n, stages_[k].delay_ticks,
                circuit::timing::leafDelay(circuit::timing::Leaf::And));
            w.connect(signal, line->getInputPin("IN"));
            signal = line->getOutputPin("OUT");
        }
        for (size_t g = 0; g < stages_[k].replica_gates; ++g)
            signal = w.both("STAGE_" + n + "_GATE_" + std::to_string(g), signal, start);
        w.connect(signal, getOutputPin("STAGE_" + n));
        previous = signal;
    }
    w.finish();
}

} // namespace circuit::memory
