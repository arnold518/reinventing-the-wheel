#pragma once

#include "components/selection/SelectionTypes.hpp"
#include "simulator/Simulator.hpp"
#include "tests/TestHelpers.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace rv32i::test {

struct ExternalFixtureRunResult {
    uint32_t tohost = 0;
    uint32_t pc = 0;
    size_t instruction_count = 0;
    size_t hardware_cycles = 0;
    SimulatorPerformanceCounters simulator_counters{};
};

struct ExternalFixtureValidationResult {
    ExternalFixtureRunResult behavioral;
    ExternalFixtureRunResult structural;
};

ExternalFixtureRunResult runExternalFixture(
    const std::filesystem::path& elf_path,
    circuit::Fidelity fidelity,
    uint32_t tohost_address,
    size_t maximum_instructions,
    bool record_history = true);

ExternalFixtureValidationResult validateExternalFixture(
    const std::filesystem::path& elf_path,
    uint32_t tohost_address,
    size_t maximum_instructions);

} // namespace rv32i::test

class RV32IExternalValidationSmokeTest
    : public StandaloneVerificationTest {
protected:
    std::string getTestName() const override;
    void verifyResults() override;
};
