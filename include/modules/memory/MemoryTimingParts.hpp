#pragma once

#include "simulator/SimulationTime.hpp"

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
    MemoryDelayLine(std::string name, SimulationTime delay, SimulationTime fall_delay = MAX_SIMULATION_TIME);
    const char* getTypeName() const override { return "MemoryDelayLine"; }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::InputsOnly; }
    void evaluate(SimulationTime time, Simulator& simulator) override;
    SimulationTime fallDelay() const { return fall_delay_; }
private:
    SimulationTime fall_delay_;
    std::vector<std::weak_ptr<Event>> pending_;
};

// A global on-chip wire carrying a bus between the subarrays of a large
// array and its periphery (an electrical stand-in leaf, decision 3 of the
// timing plan): OUT follows IN after the wire's delay, which grows with its
// length. Width 1 to 32.
class MemoryWire : public BasicComponent {
public:
    MemoryWire(std::string name, size_t width, SimulationTime delay);
    const char* getTypeName() const override { return type_.c_str(); }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::InputsOnly; }
    void evaluate(SimulationTime time, Simulator& simulator) override;
private:
    std::string type_;
};

// A shared on-chip bus between the subarrays of a large array and its
// periphery: each subarray drives it through its own tri-state buffer, and
// OUT follows the single driver after the wire's delay (an electrical
// stand-in leaf like MemoryWire). Per bit: no driver gives Z (0 on a line
// with a pull-down), drivers that agree give their value, disagreeing
// drivers give X. Width 1 to 32.
class MemoryBus : public BasicComponent {
public:
    // `pulled_low`: a line with a pull-down reads 0 when no driver is on
    // (a status line such as READY); otherwise it floats (Z).
    MemoryBus(std::string name, size_t drivers, size_t width, SimulationTime delay, bool pulled_low = false);
    const char* getTypeName() const override { return type_.c_str(); }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::InputsOnly; }
    void evaluate(SimulationTime time, Simulator& simulator) override;
private:
    size_t drivers_, width_;
    bool pulled_low_;
    std::string type_;
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
    void evaluate(SimulationTime time, Simulator& simulator) override;
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
    void evaluate(SimulationTime time, Simulator& simulator) override;
};

// KNOWN is 1 when every bit of IN is 0 or 1. Unknown-value detection is
// simulator metadata, not a gate (as in the memory request checks).
class KnownBits : public BasicComponent {
public:
    KnownBits(std::string name, size_t width);
    const char* getTypeName() const override { return "KnownBits"; }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::InputsOnly; }
    void evaluate(SimulationTime time, Simulator& simulator) override;
};

// A device's power-on reset: OUT is 1 from the start of the simulation for
// `width` ticks, then 0 for good. It stands for the analog circuit that
// holds a chip in reset while its supply comes up; a device with no reset pin
// (DRAM, NOR flash) starts idle through it.
class PowerOnReset : public BasicComponent {
public:
    PowerOnReset(std::string name, SimulationTime width);
    const char* getTypeName() const override { return "PowerOnReset"; }
    void scheduleInitialEvents(Simulator& simulator, SimulationTime time) override;
    void evaluate(SimulationTime time, Simulator& simulator) override;
private:
    SimulationTime width_;
};

// Two clocked stages that bring an asynchronous device signal (READY, BUSY)
// into the controller's clock. A stage that samples an unknown or changing
// input keeps its previous value: metastability resolves to 0 or 1 in a real
// synchronizer, and that analog resolution is not modelled. Q shows the
// second stage one flip-flop clock-to-output delay after a rising edge. RST
// high clears both stages. The timing analysis treats D as an asynchronous
// capture: no setup or hold check, no path through it.
// `stages` 1 makes a single resolving sampler (a synchronous device sampling
// the end of its own analog step), 2 a classic synchronizer.
class Synchronizer : public BasicComponent {
public:
    explicit Synchronizer(std::string name, size_t stages = 2);
    const char* getTypeName() const override { return "Synchronizer"; }
    void evaluate(SimulationTime time, Simulator& simulator) override;
private:
    size_t stages_;
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
    SimulationTime delay_ticks = 0;  // a delay line first, 0 for none
    size_t replica_gates = 1; // then this many AND gates (at least one)
};
// The settle interval a MemoryTimer actually uses. An input's change is seen
// as a pulse as long as the settle line (input against its held copy), and
// the held copy must not change before that pulse has reset the stages. The
// line is therefore at least the detection path (change detector, the
// condition gate if any, the OR tree, STEADY, STARTED and a stage's reset
// gate) plus one gate, like an address-transition detector sized against its
// own logic; below that the configured value would lose changes under
// inertial delay or let held inputs reach a still-running access.
SimulationTime memoryTimerSettle(SimulationTime configured, size_t inputs, bool conditional);

class MemoryTimer : public IOComponent {
public:
    // Inputs from `conditional_from` on restart the access only while the
    // CONDITION input is 1 (an SRAM's write data during a write). Inputs from
    // `detected` on are only held, never watched: a clocked macro starts on
    // its access token alone, while its registered address and data still
    // reach the array only once the previous access has been cleared.
    MemoryTimer(std::string name, size_t inputs, SimulationTime settle_ticks, std::vector<MemoryTimerStage> stages,
                size_t conditional_from = static_cast<size_t>(-1), size_t detected = static_cast<size_t>(-1));
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    size_t inputs_, conditional_from_, detected_;
    SimulationTime settle_;
    std::vector<MemoryTimerStage> stages_;
    std::string type_;
};

} // namespace circuit::memory
