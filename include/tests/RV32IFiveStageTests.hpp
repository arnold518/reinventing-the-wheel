#pragma once

#include "components/selection/BuildProfile.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include "rv32i/RV32IPrediction.hpp"
#include <optional>
#include <cstddef>
#include <map>
#include <memory>
#include <string>

class IOComponent;
class RV32IProgramRoot;
class RV32IStateView;
class RV32IRetirementView;
template<size_t WIDTH> class Wire;

std::string rv32iFiveStageProgramScenarioName(
    size_t program_number);

/**
 * Runs one of the shared RV32I program scenarios through the five-stage core.
 *
 * The surrounding memories are test fixtures, not another product-level
 * system component. A recursive BuildProfile still reaches the core and every
 * selectable child below it, which keeps the scenario usable by the
 * visualizer.
 */
class RV32IFiveStageCoreProgramTest
    : public RV32IInstructionLockstepTest {
public:
    explicit RV32IFiveStageCoreProgramTest(
        size_t program_number = 9, bool baseline = false,
        std::optional<rv32i::PredictionConfig> prediction = std::nullopt);

    bool run() override;
    void setupCircuit() override;
    bool isSimulationPrecomputed() const override {
        return simulation_precomputed_;
    }
    bool supportsBuildProfile() const override { return true; }
    circuit::BuildProfile getBuildProfile() const override {
        return profile_;
    }
    void setBuildProfile(circuit::BuildProfile profile) override;
    std::vector<PerformanceMetric> getPerformanceMetrics() const override;

protected:
    RV32ISystemProgramCase getCase() const override;
    void buildCircuit() override;
    void runSimulation() override;
    void initializeComponentForLockstep(
        const RV32ISystemProgramCase& test_case) override;
    void clockComponentOneCycle(
        size_t cycle_index,
        size_t cycle_start_time) override;
    void observePerformanceCycle() override;
    rv32i::RV32IState snapshotComponentState() const override;
    rv32i::RV32IMemoryTrace
    lastDataMemoryAccess() const override;
    std::map<uint32_t, uint8_t>
    lastDataMemoryWrites() const override;
    void verifyResults() override;

private:
    size_t program_number_;
    bool baseline_ = false;
    std::optional<rv32i::PredictionConfig> prediction_;
    circuit::BuildProfile profile_ =
        circuit::canonicalDefaultProfile();
    std::shared_ptr<RV32IProgramRoot> program_root_{};
    std::shared_ptr<IOComponent> core_{};
    std::shared_ptr<RV32IStateView> state_view_{};
    std::shared_ptr<IOComponent> instruction_memory_{};
    std::shared_ptr<IOComponent> data_memory_{};
    std::shared_ptr<Wire<1>> rst_wire_{};
    std::shared_ptr<Wire<1>> enable_wire_{};
    std::shared_ptr<RV32IRetirementView> retirement_view_;
    uint64_t observed_retirements_ = 0;
    rv32i::RV32IMemoryTrace retired_memory_{};
    bool simulation_precomputed_ = false;
    size_t load_use_stall_cycles_ = 0;
    size_t memory_stall_cycles_ = 0;
    size_t data_port_stall_cycles_ = 0;
    size_t pipeline_flushes_ = 0;
    size_t occupied_pipeline_slots_ = 0;
    size_t data_stall_cycles_ = 0;
    size_t branch_wait_cycles_ = 0;
    size_t branches_resolved_ = 0, branch_misses_ = 0, prediction_recoveries_ = 0;
};

class RV32IBaselinePipelineProgramTest : public RV32IFiveStageCoreProgramTest {
public:
    explicit RV32IBaselinePipelineProgramTest(size_t program=9)
        : RV32IFiveStageCoreProgramTest(program,true,std::nullopt) {}
};
