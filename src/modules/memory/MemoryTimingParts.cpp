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

MemoryDelayLine::MemoryDelayLine(std::string name, size_t delay, size_t fall_delay)
    : BasicComponent(std::move(name), delay, [](IOComponent* self) {
          self->addPin("IN", PinType::INPUT);
          self->addPin("OUT", PinType::OUTPUT);
      }), fall_delay_(std::min(fall_delay, delay)) {}

// Inertial: a new input withdraws every change still pending, so a pulse
// shorter than the delay does not pass and an edge never arrives out of order.
void MemoryDelayLine::evaluate(size_t time, Simulator& simulator) {
    auto in = getInputValue("IN");
    if (in == LogicValue::HIGH_Z) in = LogicValue::UNKNOWN;
    for (auto& event : pending_) if (auto e = event.lock()) e->cancelled = true;
    pending_.clear();
    auto pin = getOutputPin("OUT");
    pin->setBit(0, in);
    const auto wire = pin->getExternalWire();
    if (!wire) return;
    if (wire->getValueVector()[0] == in) return;
    const size_t at = time + (in == LogicValue::HIGH ? delay : fall_delay_);
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

void SymbolicDiffer::evaluate(size_t time, Simulator& simulator) {
    _updateOutputWire(simulator, "OUT", getInputValue("A") == getInputValue("B") ? LogicValue::LOW : LogicValue::HIGH, time);
}

ProtocolStableData::ProtocolStableData(std::string name, size_t width)
    : BasicComponent(std::move(name), 0, [width](IOComponent* self) {
          self->addPinDynamic("IN", PinType::INPUT, width);
          self->addPinDynamic("OUT", PinType::OUTPUT, width);
      }) {}

void ProtocolStableData::evaluate(size_t time, Simulator& simulator) {
    _updateOutputWireDynamic(simulator, "OUT", getInputPinDynamic("IN")->getValueAsVector(), time);
}

KnownBits::KnownBits(std::string name, size_t width)
    : BasicComponent(std::move(name), circuit::timing::leafDelay(circuit::timing::Leaf::MemoryPrimitive), [width](IOComponent* self) {
          self->addPinDynamic("IN", PinType::INPUT, width);
          self->addPin("KNOWN", PinType::OUTPUT);
      }) {}

void KnownBits::evaluate(size_t time, Simulator& simulator) {
    bool known = true;
    for (auto bit : getInputPinDynamic("IN")->getValueAsVector()) known = known && (bit == LogicValue::LOW || bit == LogicValue::HIGH);
    _updateOutputWire(simulator, "KNOWN", known ? LogicValue::HIGH : LogicValue::LOW, time);
}

PowerOnReset::PowerOnReset(std::string name, size_t width)
    : BasicComponent(std::move(name), 0, [](IOComponent* self) { self->addPin("OUT", PinType::OUTPUT); }), width_(width) {}

void PowerOnReset::scheduleInitialEvents(Simulator& simulator, size_t time) {
    auto pin = getOutputPin("OUT");
    pin->setBit(0, LogicValue::HIGH);
    if (auto wire = pin->getExternalWire()) {
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time, wire, LogicValue::HIGH));
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time + width_, wire, LogicValue::LOW));
    }
    pin->setBit(0, LogicValue::LOW);
}

void PowerOnReset::evaluate(size_t, Simulator&) {}

Synchronizer::Synchronizer(std::string name, size_t stages)
    : BasicComponent(std::move(name), circuit::dflipflop_timing::clockToOutputMaximum(), [](IOComponent* self) {
          for (const auto* pin : {"CLK", "RST", "D"}) self->addPin(pin, PinType::INPUT);
          self->addPin("Q", PinType::OUTPUT);
      }), stages_(stages) {
    if (stages != 1 && stages != 2) throw std::invalid_argument("A synchronizer has one or two stages");
}

void Synchronizer::evaluate(size_t time, Simulator& simulator) {
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

MemoryWire::MemoryWire(std::string name, size_t width, size_t delay)
    : BasicComponent(std::move(name), delay, [width](IOComponent* self) {
          if (!width || width > 32) throw std::invalid_argument("A memory wire carries 1 to 32 bits");
          self->addPinDynamic("IN", PinType::INPUT, width);
          self->addPinDynamic("OUT", PinType::OUTPUT, width);
      }),
      type_("MemoryWire<" + std::to_string(width) + ">") {
    if (!delay) throw std::invalid_argument("A memory wire needs a positive delay");
}

void MemoryWire::evaluate(size_t time, Simulator& simulator) {
    _updateOutputWireDynamic(simulator, "OUT", getInputPinDynamic("IN")->getValueAsVector(), time);
}

size_t memoryTimerSettle(size_t configured, size_t inputs, bool conditional) {
    using circuit::timing::Leaf;
    using circuit::timing::leafDelay;
    size_t levels = 0; while ((size_t{1} << levels) < inputs) ++levels;
    const size_t detection = leafDelay(Leaf::Xor) + (conditional ? leafDelay(Leaf::And) : 0) + levels * leafDelay(Leaf::Or)
        + leafDelay(Leaf::Not) + 3 * leafDelay(Leaf::And);
    return std::max(configured, detection);
}

MemoryTimer::MemoryTimer(std::string name, size_t inputs, size_t settle_ticks, std::vector<MemoryTimerStage> stages,
                         size_t conditional_from)
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
      inputs_(inputs), settle_(memoryTimerSettle(settle_ticks, inputs, conditional_from < inputs)),
      conditional_from_(conditional_from), stages_(std::move(stages)) {
    if (!settle_ticks) throw std::invalid_argument("A memory timer needs a positive settle delay");
    for (const auto& stage : stages_)
        if (!stage.replica_gates) throw std::invalid_argument("Every timer stage needs at least one gate");
    type_ = "MemoryTimer<" + std::to_string(inputs_) + "," + std::to_string(stages_.size()) + ">";
}

void MemoryTimer::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    std::vector<circuit::detail::StructuralWiring::PinPtr> changes;
    for (size_t i = 0; i < inputs_; ++i) {
        const auto n = std::to_string(i);
        auto held = b.addNewComponent<MemoryDelayLine>("SETTLE_" + n, settle_);
        w.connect(getInputPin("IN_" + n), held->getInputPin("IN"));
        w.connect(held->getOutputPin("OUT"), getOutputPin("HELD_" + n));
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
