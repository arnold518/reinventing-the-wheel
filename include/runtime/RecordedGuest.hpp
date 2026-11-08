#pragma once
#include "system/ComputerSystem.hpp"
#include "simulator/Simulator.hpp"
#include <string>
#include <functional>

class SignalTraceObserver;

namespace circuit::runtime {
struct ButtonInput { SimulationTime tick; uint64_t sequence; uint8_t button; bool down; };
struct ButtonTransition { uint8_t button; bool down; };
// What a checked run reports as it goes, for a scenario's timeline.
struct GuestRunEvent {
    enum class Kind { ResetReleased, InputSampled, Presented, ConsoleLine, Halted };
    Kind kind;
    SimulationTime time = 0;              // settled state, before the next edge
    uint64_t cycle = 0, instructions = 0;
    uint32_t value = 0;                   // button | down << 8, frame sequence, LED pins
    std::string text;                     // a console line, without its newline
};
struct RecordedGuestOptions {
    // A built-in system description (systems/*.json) with the games
    // machine's register map, and its options.
    std::string system = "games-machine";
    circuit::system::GameMachineOptions machine;
    SimulationTime start_time = 0;
    SimulationTime period = 1000, reset_release = 2000, origin = 3000;
    size_t quantum_events = 100000;
    size_t max_cycles = 500000, max_instructions = 200000, max_frames = 64;
    size_t max_console_bytes = 65536, max_device_reads = 65536;
    uint64_t max_events = 500000000;
    bool compact_children = true, capture_frames = true;
    // Expand the CPU assembly while retaining the selected compact children.
    bool expanded_core = false;
    // Empty preserves the legacy compact/expanded flags for existing callers.
    // Named presets are shared with the normal visualizer.
    std::string fidelity_profile;
    // Scripted batches, one after each PRESENT, sampled at successive safe
    // falling-edge boundaries. Exclusive with absolute-tick input. Observers
    // stay read-only; game logic still executes entirely in the guest CPU.
    std::vector<std::vector<ButtonTransition>> presentation_inputs;
    // Observation only, called every 4096 cycles after settled propagation.
    std::function<void(uint64_t cycles,uint64_t instructions,uint64_t presentations)> progress;
    // Read-only observation after a checked PRESENT has settled. No history is
    // enabled, and the observer must neither drive inputs nor advance time.
    std::function<void(const std::shared_ptr<Component>&, const Simulator&, SimulationTime)> presentation_observer;
    // Subscribe before the first scheduled event. The runtime owns the
    // subscription; the observer streams data without enabling histories.
    std::function<std::shared_ptr<SignalTraceObserver>(const std::shared_ptr<Component>&, const Simulator&)> signal_trace;
    // Observation only: reset release, sampled inputs, frames, console lines
    // and the halt, each after its cycle has settled.
    std::function<void(const GuestRunEvent&)> on_event;
    // Saves (circuit::state::saveMachine plus the runner's own checks). A
    // save is taken at the top of a cycle: the circuit has settled through
    // the end of the previous cycle and the cycle's clock edges are not yet
    // queued. `save_at` is asked at every cycle after the first, and once more
    // when stop_after_cycles ends the run (the top of the cycle it stops at).
    //   Machine:     the machine and the runner's models (a replay starts here).
    //   WithResults: also the results so far (frames, inputs, device reads),
    //                so a resumed run reports the whole session.
    enum class SaveContent { None, Machine, WithResults };
    std::function<SaveContent(uint64_t cycle, uint64_t presentations)> save_at;
    // settled: the last tick of the previous cycle, when the save's state holds.
    std::function<void(uint64_t cycle, SimulationTime settled, SaveContent content, std::vector<uint8_t> save)> on_save;
    // Continue from a save of the same runner build, options, program and
    // input, into a freshly built machine. Every check continues.
    std::shared_ptr<const std::vector<uint8_t>> resume;
    // The machine is already in the save's state (a session continuing where
    // its last run stopped): only the runner's own state is taken from it.
    bool resume_in_place = false;
    // Stop without error once this many cycles (counted from cycle 0) have
    // run; 0 runs to the program's halt.
    size_t stop_after_cycles = 0;
};
struct DeviceInputSnapshot { uint32_t levels=0,count=0,head=0; bool overflow=false; };
struct DeviceReadSample { SimulationTime tick; uint32_t address, value; };
struct RecordedGuestResult {
    uint64_t cycles = 0, instructions = 0, setup_violations = 0;
    double setup_seconds = 0, execution_seconds = 0, capture_seconds = 0;
    uint64_t presentations = 0, changed_frames = 0;
    std::string profile_name, profile_fingerprint;
    std::string fidelity_profile, build_manifest;
    std::string system;
    circuit::system::GameMachineOptions configuration;
    SimulationTime period = 0, start_time = 0;
    size_t quantum_events = 0;
    std::vector<uint64_t> frame_cycles, frame_instructions;
    std::vector<double> frame_seconds;
    size_t ram_allocated_bytes = 0, video_allocated_bytes = 0;
    std::string console;
    std::vector<FrameSnapshot> frames;
    std::vector<DeviceInputSnapshot> frame_inputs;
    // LED pins at each frame, when the system has an LED panel.
    std::vector<uint32_t> frame_leds;
    std::vector<ButtonInput> applied_inputs;
    std::vector<DeviceReadSample> device_reads;
    std::vector<uint8_t> report, report_known;
    rv32i::RV32IState state;
    SimulatorPerformanceCounters events;
    // A resumed run: the cycle it started at. Stopped: ended by stop_after_cycles.
    uint64_t resumed_cycle = 0;
    bool stopped = false;
};
// Batch only. Every memory/I/O instruction is checked against independent ISA
// and device models. Failure or budget exhaustion throws; no partial success.
RecordedGuestResult runRecordedGuest(const std::vector<uint8_t>& elf,
    const std::vector<ButtonInput>& input, const RecordedGuestOptions& = {});
// The `system.computer` parameters for these options.
circuit::ParameterMap recordedMachineParameters(const RecordedGuestOptions& o);
// The same checked run on a machine the caller built (with its own build
// profile) and a simulator the caller owns, for example with wire history on
// for the visualizer. `o.machine` must describe the machine's options.
RecordedGuestResult runRecordedGuestOn(const std::shared_ptr<circuit::system::ComputerSystem>& machine,
    Simulator& sim, const std::vector<uint8_t>& elf, const std::vector<ButtonInput>& input,
    const RecordedGuestOptions& o);
// A run save's cycle, and its machine section (circuit::state::saveMachine),
// for either save version.
uint64_t savedRunCycle(const std::vector<uint8_t>& save);
// 1 or 2; only a version 2 save can continue in place.
uint32_t savedRunVersion(const std::vector<uint8_t>& save);
std::vector<uint8_t> savedMachine(const std::vector<uint8_t>& save);
std::string recordedGuestJson(const RecordedGuestResult&);
}
