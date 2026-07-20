#pragma once

#include "rv32i/RV32IInstructionTrace.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include "simulator/SimulationTest.hpp"
#include <map>
#include <memory>

class RV32ISingleCycleSystem;
template<size_t WIDTH> class Wire;

class RV32ISingleCycleCoreSmokeTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;

private:
    std::shared_ptr<RV32ISingleCycleSystem> system_{};
    std::shared_ptr<Wire<1>> clk_wire_{};
    std::shared_ptr<Wire<1>> rst_wire_{};
    std::shared_ptr<Wire<1>> enable_wire_{};
};

class RV32ISingleCycleSystemContractTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;

private:
    std::shared_ptr<RV32ISingleCycleSystem> system_{};
    std::shared_ptr<Wire<1>> clk_wire_{};
    std::shared_ptr<Wire<1>> rst_wire_{};
    std::shared_ptr<Wire<1>> enable_wire_{};
};

class RV32ISingleCycleSystemProgramTestBase : public RV32IInstructionLockstepTest {
public:
    void setupCircuit() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    void buildCircuit() override;
    void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) override;
    void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) override;
    rv32i::RV32IState snapshotComponentState() const override;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const override;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const override;
    void verifyResults() override;

private:
    std::shared_ptr<RV32ISingleCycleSystem> system_{};
    std::shared_ptr<Wire<1>> clk_wire_{};
    std::shared_ptr<Wire<1>> rst_wire_{};
    std::shared_ptr<Wire<1>> enable_wire_{};
    rv32i::RV32IMemoryTrace last_access_{};
    uint64_t committed_instruction_count_ = 0;
    size_t visual_run_duration_ = 0;
    size_t visual_time_origin_ = 0;
    mutable size_t last_observed_memory_time_ = 0;
};

#define DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(NUMBER) \
class RV32ISingleCycleSystemProgram##NUMBER##Test : public RV32ISingleCycleSystemProgramTestBase { \
protected: \
    RV32ISystemProgramCase getCase() const override; \
};

DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(1)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(2)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(3)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(4)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(5)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(6)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(7)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(8)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(9)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(10)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(11)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(12)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(13)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(14)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(15)
DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST(16)

#undef DECLARE_STRUCTURAL_RV32I_PROGRAM_TEST
