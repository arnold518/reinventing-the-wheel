#pragma once

#include "basic/Wire.hpp"
#include "modules/rv32i/RV32IReferenceSystem.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include "tests/RV32IProgramCases.hpp"
#include <memory>

class RV32IReferenceSystemProgramTestBase : public RV32IInstructionLockstepTest {
public:
    void setupCircuit() override;
    size_t getRunDuration() const override;

protected:
    void buildCircuit() override;
    RV32ISystemProgramCase getCase() const override = 0;
    void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) override;
    void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) override;
    rv32i::RV32IState snapshotComponentState() const override;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const override;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const override;
    std::shared_ptr<Memory64Kx32> dataMemoryForTest() const;
    void verifyResults() override;

private:
    std::shared_ptr<RV32IReferenceSystem> system_{};
    std::shared_ptr<Wire<>> clk_wire_{};
    std::shared_ptr<Wire<>> rst_wire_{};
    std::shared_ptr<Wire<>> enable_wire_{};
    size_t visual_run_duration_ = 0;
    mutable size_t last_observed_memory_time_ = 0;
};

class RV32IReferenceSystemContractTest : public SimulationTest {
public:
    void setupCircuit() override;

protected:
    std::string getTestName() const override;
    void buildCircuit() override;
    void setInitialState() override;
    void runSimulation() override;
    void verifyResults() override;

private:
    std::shared_ptr<RV32IReferenceSystem> system_{};
    std::shared_ptr<Wire<>> clk_wire_{};
    std::shared_ptr<Wire<>> rst_wire_{};
    std::shared_ptr<Wire<>> enable_wire_{};
    std::shared_ptr<Wire<32>> pc_wire_{};
    std::shared_ptr<Wire<>> halted_wire_{};
    std::shared_ptr<Wire<>> trapped_wire_{};
    bool verified_ = false;
};

class RV32IReferenceSystemProgram1Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram2Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
    void verifyResults() override;
};

class RV32IReferenceSystemProgram3Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram4Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram5Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
    void verifyResults() override;
};

class RV32IReferenceSystemProgram6Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram7Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram8Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram9Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram10Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram11Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram12Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram13Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram14Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram15Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};

class RV32IReferenceSystemProgram16Test : public RV32IReferenceSystemProgramTestBase {
public:
    RV32ISystemProgramCase getCase() const override;
};
