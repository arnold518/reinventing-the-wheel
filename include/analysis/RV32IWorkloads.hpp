#pragma once

#include "analysis/RV32IBenchmarks.hpp"
#include "benchmark/Benchmark.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace circuit::analysis {

enum class RV32IWorkloadCompletion { TerminalState, TohostVerdict };

struct RV32IWorkloadBlock {
    bool instruction = false;
    uint32_t address = 0;
    std::vector<uint8_t> bytes;
};

struct RV32IWorkloadRequest {
    size_t program = 20;
    std::string workload;
    std::string elf;
    size_t iterations = 128;
    RV32IBenchmarkOptions benchmark;
    std::optional<uint32_t> start_pc;
    std::optional<uint32_t> stop_pc;
    uint32_t tohost = 0x3ffc0;
    size_t capacity_bytes = 256 * 1024;
};

struct RV32IPreparedWorkload {
    benchmark::WorkloadDescriptor descriptor;
    std::vector<RV32IWorkloadBlock> blocks;
    RV32IWorkloadCompletion completion = RV32IWorkloadCompletion::TerminalState;
    uint32_t tohost = 0x3ffc0;
    std::optional<uint32_t> measurement_start_pc;
    std::optional<uint32_t> measurement_stop_pc;
    size_t training_iterations = 0;
    std::optional<uint32_t> expected_result;
    std::vector<RV32IBenchmarkData> expected_data;
    std::vector<uint32_t> branch_pcs;
    std::string fingerprint;
};

const std::vector<std::string>& rv32iGeneratedWorkloadNames();
const std::vector<benchmark::WorkloadDescriptor>& rv32iWorkloadCatalog();
bool isRV32IGeneratedWorkload(const std::string& name);
RV32IPreparedWorkload prepareRV32IWorkload(const RV32IWorkloadRequest& request);

} // namespace circuit::analysis
