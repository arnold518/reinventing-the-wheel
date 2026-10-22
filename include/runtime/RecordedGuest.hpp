#pragma once
#include "modules/rv32im/RV32IMDeviceSystem.hpp"
#include "simulator/Simulator.hpp"
#include <string>
#include <functional>

namespace circuit::runtime {
struct ButtonInput { SimulationTime tick; uint64_t sequence; uint8_t button; bool down; };
struct RecordedGuestOptions {
    rv32im::DeviceSystemConfig machine;
    SimulationTime start_time = 0;
    SimulationTime period = 1000, reset_release = 2000, origin = 3000;
    size_t quantum_events = 100000;
    size_t max_cycles = 500000, max_instructions = 200000, max_frames = 64;
    size_t max_console_bytes = 65536, max_device_reads = 65536;
    uint64_t max_events = 500000000;
    bool compact_children = true, capture_frames = true;
    // Expand the CPU assembly while retaining the selected compact children.
    bool expanded_core = false;
    // Observation only, called every 4096 cycles after settled propagation.
    std::function<void(uint64_t cycles,uint64_t instructions,uint64_t presentations)> progress;
    // Read-only observation after a checked PRESENT has settled. No history is
    // enabled, and the observer must neither drive inputs nor advance time.
    std::function<void(const std::shared_ptr<Component>&, const Simulator&, SimulationTime)> presentation_observer;
};
struct DeviceInputSnapshot { uint32_t levels=0,count=0,head=0; bool overflow=false; };
struct DeviceReadSample { SimulationTime tick; uint32_t address, value; };
struct RecordedGuestResult {
    uint64_t cycles = 0, instructions = 0, setup_violations = 0;
    double setup_seconds = 0, execution_seconds = 0, capture_seconds = 0;
    uint64_t presentations = 0, changed_frames = 0;
    std::string profile_name, profile_fingerprint;
    rv32im::DeviceSystemConfig configuration;
    SimulationTime period = 0, start_time = 0;
    size_t quantum_events = 0;
    std::vector<uint64_t> frame_cycles, frame_instructions;
    std::vector<double> frame_seconds;
    size_t ram_allocated_bytes = 0, video_allocated_bytes = 0;
    std::string console;
    std::vector<FrameSnapshot> frames;
    std::vector<DeviceInputSnapshot> frame_inputs;
    std::vector<ButtonInput> applied_inputs;
    std::vector<DeviceReadSample> device_reads;
    std::vector<uint8_t> report, report_known;
    rv32i::RV32IState state;
    SimulatorPerformanceCounters events;
};
// Batch only. Every memory/I/O instruction is checked against independent ISA
// and device models. Failure or budget exhaustion throws; no partial success.
RecordedGuestResult runRecordedGuest(const std::vector<uint8_t>& elf,
    const std::vector<ButtonInput>& input, const RecordedGuestOptions& = {});
std::string recordedGuestJson(const RecordedGuestResult&);
}
