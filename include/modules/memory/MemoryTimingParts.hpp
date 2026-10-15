#pragma once

#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace circuit::memory {

// An analog interval of a memory device (bitline precharge, sense
// development, DRAM restore, a flash program or erase pulse) as one leaf:
// OUT follows IN after `delay` ticks. It stands for electrical behavior that
// gates cannot express; the device chooses the delay from the timing profile
// and its geometry. Inertial: a change of IN withdraws a pending, opposite
// change of OUT, so a restarted access never sees a stale edge.
//
// `fall_delay` below `delay` makes a reset-dominant line, as self-timed pulse
// chains are built: a rise takes the whole interval, a fall passes quickly,
// so a restarted sequence never sees a late edge of the previous one.
class MemoryDelayLine : public BasicComponent {
public:
    MemoryDelayLine(std::string name, size_t delay, size_t fall_delay = static_cast<size_t>(-1));
    const char* getTypeName() const override { return "MemoryDelayLine"; }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::InputsOnly; }
    void evaluate(size_t time, Simulator& simulator) override;
    size_t fallDelay() const { return fall_delay_; }
private:
    size_t fall_delay_;
    std::vector<std::weak_ptr<Event>> pending_;
};

// The XOR of an address-transition detector, with the simulator's unknown
// value read as a symbol: OUT is 1 when A and B differ (0, 1, X and Z as four
// symbols), 0 when they are equal. A steady unknown input is steady; a
// plain XOR gate would report X forever. Delay of an XOR gate.
class SymbolicDiffer : public BasicComponent {
public:
    explicit SymbolicDiffer(std::string name);
    const char* getTypeName() const override { return "SymbolicDiffer"; }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::InputsOnly; }
    void evaluate(size_t time, Simulator& simulator) override;
protected:
    bool inertialDelay() const override { return true; }
};

// A timing exception made visible as a part: OUT is IN, through wires only.
// The protocol keeps IN stable for at least one clock cycle before it is
// captured (a memory's read data, which the controller takes only after the
// device's READY has passed a two-stage synchronizer), so the timing analysis
// draws no single-cycle path through it: IN is unused and OUT stable. Real
// designs state the same as a multicycle-path constraint.
class ProtocolStableData : public BasicComponent {
public:
    ProtocolStableData(std::string name, size_t width);
    const char* getTypeName() const override { return "ProtocolStableData"; }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::InputsOnly; }
    void evaluate(size_t time, Simulator& simulator) override;
};

// Two clocked stages that bring an asynchronous device signal (READY, BUSY)
// into the controller's clock. A stage that samples an unknown or changing
// input keeps its previous value: metastability resolves to 0 or 1 in a real
// synchronizer, and that analog resolution is not modelled. Q shows the
// second stage one flip-flop clock-to-output delay after a rising edge. RST
// high clears both stages. The timing analysis treats D as an asynchronous
// capture: no setup or hold check, no path through it.
class Synchronizer : public BasicComponent {
public:
    explicit Synchronizer(std::string name);
    const char* getTypeName() const override { return "Synchronizer"; }
    void evaluate(size_t time, Simulator& simulator) override;
private:
    LogicValue first_ = LogicValue::LOW, second_ = LogicValue::LOW, previous_clock_ = LogicValue::UNKNOWN;
};

// The self-timing of an asynchronous memory access, built from gates and
// delay lines. Each input IN_i is copied through a settle delay line to
// HELD_i; the downstream circuit reads HELD, so it keeps seeing the old value
// while a change propagates. An input that differs from its held copy marks
// the access as changing (address-transition detection with XOR gates and an
// OR tree). STAGE_0 = ENABLE and not changing; STAGE_k follows STAGE_(k-1)
// through stage k's delay line (if any) and its replica gate chain, each
// gate also ANDed with STAGE_0, so any change or disable clears every stage at
// once and the sequence restarts when the inputs are steady again. A replica
// chain of AND gates copies the depth of a real path (for example the row
// decoder) so the stage ends when that path has settled, in every timing
// profile. Unknown inputs make the stages unknown.
struct MemoryTimerStage {
    size_t delay_ticks = 0;  // a delay line first, 0 for none
    size_t replica_gates = 1; // then this many AND gates (at least one)
};
class MemoryTimer : public IOComponent {
public:
    // Inputs from `conditional_from` on restart the access only while the
    // CONDITION input is 1 (an SRAM's write data during a write).
    MemoryTimer(std::string name, size_t inputs, size_t settle_ticks, std::vector<MemoryTimerStage> stages,
                size_t conditional_from = static_cast<size_t>(-1));
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    size_t inputs_, settle_, conditional_from_;
    std::vector<MemoryTimerStage> stages_;
    std::string type_;
};

} // namespace circuit::memory
