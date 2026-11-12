#pragma once
// Runs a chapter 6 guest program on the accelerator machine
// (systems/accelerator-machine*.json) and reads its report page.
//
// Unlike the checked game runner of chapter 5 this does not compare every
// instruction with an ISA model (the unit's DMA writes memory behind the
// core's back); the program's results are checked against a host reference
// from the report page instead.
#include "simulator/SimulationTime.hpp"
#include "components/selection/BuildProfile.hpp"
#include <optional>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Simulator;
namespace circuit::system { class ComputerSystem; }

namespace circuit::runtime {

struct AcceleratorRunOptions {
    size_t matrix = 16;              // MatrixUnit<N>; 0: the machine without the unit
    std::string memory = "sram";     // main memory: ff, sram or dram
    SimulationTime dram_retention = 0;   // dram: row retention in ticks; 0: the timing profile's
    bool array_multiplier = false;   // RV32IM core multiplier=array (§ 6.1.2)
    bool pipelined_bus = false;      // the § 6.3 bus (two requests in flight)
    std::string fidelity;            // empty: compact; else a named preset (gate, logic, visual, compact)
    std::optional<BuildProfile> profile;   // overrides `fidelity` when set
    SimulationTime period = 1000, start_time = 0, reset_release = 2000, origin = 3000;
    uint64_t max_cycles = 100'000'000;
    // A run with no completed instruction for this many cycles stops with an
    // error that lists the machine's bus handshake pins (0: never). A run
    // whose commit signal turns unknown (X) stops at once the same way.
    uint64_t stall_cycles = 1'000'000;
    size_t quantum_events = 100000;
    std::function<void(uint64_t cycles, uint64_t instructions)> progress;   // once when running starts, then every 4,096 cycles
    // Timeline marks: reset released, each console line, the halt.
    std::function<void(SimulationTime, const std::string& label, const std::string& detail)> on_event;
};

struct AcceleratorRunResult {
    uint64_t cycles = 0, instructions = 0, setup_violations = 0, events = 0;
    double setup_seconds = 0, run_seconds = 0;
    bool halted = false, trapped = false;
    uint32_t trap_cause = 0;
    std::string console;
    std::vector<uint8_t> report;     // 4 KiB from 0x3f000
    SimulationTime end_time = 0;
    uint32_t word(size_t index) const;
};

// The description text for these options (machine, unit size, memory, core).
std::string acceleratorMachineDescription(const AcceleratorRunOptions&);
std::shared_ptr<system::ComputerSystem> buildAcceleratorMachine(const AcceleratorRunOptions&, const std::string& root = "MACHINE");
AcceleratorRunResult runAcceleratorGuest(const std::vector<uint8_t>& elf, const AcceleratorRunOptions&);
AcceleratorRunResult runAcceleratorGuestOn(const std::shared_ptr<system::ComputerSystem>&, Simulator&,
                                           const std::vector<uint8_t>& elf, const AcceleratorRunOptions&);
// tests/fixtures/accelerator/<name>.elf, embedded in the build (no file is read at run time).
std::vector<uint8_t> acceleratorFixture(const std::string& name);

} // namespace circuit::runtime
