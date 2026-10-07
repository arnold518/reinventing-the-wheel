#pragma once
#include "rv32i/RV32IProgram.hpp"
#include <string>
#include <vector>

namespace circuit::analysis {
struct RV32IBenchmarkOptions {
    size_t size=16, distance=1, branch_spacing=16, training_iterations=0;
    uint32_t seed=1;
    std::string pattern="alternating";
    bool measure_body=false;
    bool operator==(const RV32IBenchmarkOptions&) const = default;
};
struct RV32IBenchmarkData { uint32_t address; std::vector<uint8_t> bytes; };
struct RV32IBenchmarkImage {
    rv32i::RV32IProgram program;
    std::vector<RV32IBenchmarkData> initial_data, expected_data;
    uint32_t expected_result=0, body_begin=0, body_end=0;
    std::vector<uint32_t> branch_pcs;
    std::string description;
};
const std::vector<std::string>& rv32iBenchmarkNames();
bool isRV32IBenchmark(const std::string& name);
void validateRV32IBenchmark(const std::string& name, size_t iterations, const RV32IBenchmarkOptions&);
RV32IBenchmarkImage buildRV32IBenchmark(const std::string& name, size_t iterations, const RV32IBenchmarkOptions&);
}
