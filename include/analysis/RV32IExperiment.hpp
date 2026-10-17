#pragma once
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "analysis/RV32IBenchmarks.hpp"
#include "benchmark/Benchmark.hpp"
#include "modules/memory/Cache.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "rv32i/RV32IPrediction.hpp"
#include "rv32i/RV32IState.hpp"
#include "simulator/Simulator.hpp"
#include <map>
#include <functional>
#include <string>
#include <vector>

namespace circuit::analysis {
struct RV32IExperimentProgress {
    size_t cycles = 0;
    size_t instruction_attempts = 0;
    size_t retired_instructions = 0;
    double host_seconds = 0.0;
};
// A cache of the memory-topology campaign: `sets` sets of two ways and
// 16-byte lines, SRAM tags and data, scalable storage, burst fills.
inline circuit::memory::CacheConfig campaignCache(size_t sets, bool writable) {
    circuit::memory::CacheConfig c;
    c.sets=sets;c.ways=2;c.block_bytes=16;c.backing_depth_words=65536;c.read_write=writable;
    c.tag_technology=c.data_technology=circuit::memory::Technology::SRAM;
    c.scalable_storage=true;c.burst_fill=true;
    return c;
}

struct RV32IExperimentConfig {
    std::string core="advanced"; // single, baseline, advanced
    std::string fidelity="behavioral"; // behavioral or reference (shared structural profile)
    size_t program=20, period=4000, max_cycles=100000, max_events=100000000;
    rv32i::PredictionConfig prediction{};
    rv32i::MemoryConfiguration memory{};
    // Memory topology (advanced core): empty = the two separate memories
    // with each image part in its own memory; "A" = two separate memories,
    // each with the whole image; "B" = both ports through the two-port
    // arbiter to one memory; "C-ID" = an L1 per port, then the arbiter;
    // "D-0" = the arbiter, then a shared L2; "D-ID" = L1s, arbiter, L2.
    std::string topology;
    circuit::memory::CacheConfig instruction_l1=campaignCache(8,false), data_l1=campaignCache(8,true),
        l2=campaignCache(64,true);
    std::string elf;
    std::string workload; // legacy loops or rv32iBenchmarkNames(); empty selects program/ELF
    size_t iterations=128;
    RV32IBenchmarkOptions benchmark;
    std::optional<uint32_t> start_pc, stop_pc; // inclusive/exclusive retirement PCs
    uint32_t tohost=0x3ffc0;
    bool verify=true, safe_clock_only=false;
    bool binary_startup=false; // only with safe_clock_only; explicit narrower contract
    bool allow_incomplete=false; // bounded calibration run, never a passing benchmark
    // Opt-in settled pre-edge signals plus post-edge architectural retirement.
    // Restricted to <=4096 cycles; ordinary performance runs retain no trace.
    bool trace_cycles=false;
    // Validation only, gate-level core: every cycle, the memory handshake as
    // simulated must be one of the states the clock analysis went through.
    bool check_handshake_states=false;
    size_t progress_interval_cycles=0;
    std::function<void(const RV32IExperimentProgress&)> progress;
    void validate() const;
};
struct RV32IExperimentResult {
    size_t cycles=0, attempts=0, retired=0, terminal_attempts=0;
    size_t whole_program_cycles=0;
    std::map<std::string,uint64_t> observations;
    SimulatorPerformanceCounters simulator;
    rv32i::RV32IState final_state;
    ClockTimingReport safe_clock;
    std::string manifest, workload_fingerprint, state_fingerprint;
    std::string workload_description;
    std::optional<uint32_t> benchmark_expected_result, benchmark_result;
    std::optional<uint32_t> measurement_start_pc, measurement_stop_pc;
    std::vector<uint32_t> benchmark_branch_pcs;
    double setup_seconds=0, execution_seconds=0, verification_seconds=0;
    bool completed=false, prefix_verified=false, verified=false;
    size_t handshake_states_checked=0;
    benchmark::RunRecord benchmark_run;
    std::vector<std::map<std::string,uint64_t>> cycle_trace;
};
RV32IExperimentResult runRV32IExperiment(const RV32IExperimentConfig&);
}
