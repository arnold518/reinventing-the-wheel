#pragma once
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "analysis/SafeClockTiming.hpp"
#include "analysis/RV32IBenchmarks.hpp"
#include "benchmark/Benchmark.hpp"
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
struct RV32IExperimentConfig {
    std::string core="advanced"; // single, baseline, advanced
    std::string fidelity="behavioral"; // behavioral or reference (shared structural profile)
    size_t program=20, period=4000, max_cycles=100000, max_events=100000000;
    rv32i::PredictionConfig prediction{};
    rv32i::MemoryConfiguration memory{};
    std::string elf;
    std::string workload; // legacy loops or rv32iBenchmarkNames(); empty selects program/ELF
    size_t iterations=128;
    RV32IBenchmarkOptions benchmark;
    std::optional<uint32_t> start_pc, stop_pc; // inclusive/exclusive retirement PCs
    uint32_t tohost=0x3ffc0;
    bool verify=true, timing_only=false, safe_clock_only=false;
    bool binary_startup=false; // only with safe_clock_only; explicit narrower contract
    bool allow_incomplete=false; // bounded calibration run, never a passing benchmark
    // Opt-in settled pre-edge signals plus post-edge architectural retirement.
    // Restricted to <=4096 cycles; ordinary performance runs retain no trace.
    bool trace_cycles=false;
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
    CircuitTimingReport timing;
    ClockTimingReport safe_clock;
    std::string manifest, workload_fingerprint, state_fingerprint;
    std::string workload_description;
    std::optional<uint32_t> benchmark_expected_result, benchmark_result;
    std::optional<uint32_t> measurement_start_pc, measurement_stop_pc;
    std::vector<uint32_t> benchmark_branch_pcs;
    double setup_seconds=0, execution_seconds=0, verification_seconds=0;
    bool completed=false, prefix_verified=false, verified=false;
    benchmark::RunRecord benchmark_run;
    std::vector<std::map<std::string,uint64_t>> cycle_trace;
};
RV32IExperimentResult runRV32IExperiment(const RV32IExperimentConfig&);
}
