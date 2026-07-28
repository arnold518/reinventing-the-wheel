#pragma once

#include "rv32i/RV32IInstructionTrace.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include "tests/ComponentTestModel.hpp"
#include "simulator/SimulationTest.hpp"
#include "components/capabilities/RV32ISystemProgramAccess.hpp"
#include <map>
#include <memory>

class RV32ISingleCycleSystem;
template<size_t WIDTH> class Wire;

class RV32ISingleCycleCoreTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32ISingleCycleCoreTest();
    bool run() override;
};

class RV32ISystemProfileRun : public RV32IInstructionLockstepTest {
public:
    void setupCircuit() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
    bool supportsBuildProfile() const override { return true; }
    circuit::BuildProfile getBuildProfile() const override {
        return profile_;
    }
    void setBuildProfile(circuit::BuildProfile profile) override;

protected:
    void buildCircuit() override;
    virtual bool useRepresentativeProfile() const { return false; }
    void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) override;
    void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) override;
    rv32i::RV32IState snapshotComponentState() const override;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const override;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const override;
    void verifyResults() override;

private:
    std::shared_ptr<IOComponent> system_{};
    std::shared_ptr<RV32ISystemProgramAccess> program_access_{};
    std::shared_ptr<RV32ISingleCycleSystem> structural_system_{};
    std::shared_ptr<Wire<1>> clk_wire_{};
    std::shared_ptr<Wire<1>> rst_wire_{};
    std::shared_ptr<Wire<1>> enable_wire_{};
    rv32i::RV32IMemoryTrace last_access_{};
    uint64_t committed_instruction_count_ = 0;
    size_t visual_run_duration_ = 0;
    size_t visual_time_origin_ = 0;
    mutable size_t last_observed_memory_time_ = 0;
    circuit::BuildProfile profile_ =
        circuit::withExactFidelity(
            circuit::canonicalDefaultProfile(),
            "RV32I_SINGLE_CYCLE_SYSTEM_ROOT",
            circuit::Fidelity::Structural,
            "rv32i-structural-system-test");
};

class RV32ISingleCycleSystemTest
    : public RV32ISystemProfileRun {
public:
    explicit RV32ISingleCycleSystemTest(size_t program_number = 9);
    bool run() override;

protected:
    RV32ISystemProgramCase getCase() const override;

private:
    size_t program_number_;
};

class RV32IProfileToggleSweepTest
    : public StandaloneVerificationTest {
protected:
    std::string getTestName() const override;
    void verifyResults() override;
};
