#pragma once

#include "basic/Wire.hpp"
#include "modules/rv32i/RV32ISystem.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include <memory>

class BehavioralRV32ISystemProgramTestBase : public RV32IInstructionLockstepTest {
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

private:
    std::shared_ptr<RV32ISystem> system_{};
    std::shared_ptr<Wire<>> clk_wire_{};
    std::shared_ptr<Wire<>> rst_wire_{};
    std::shared_ptr<Wire<>> enable_wire_{};
    size_t visual_run_duration_ = 0;
};

class BehavioralRV32ISystemProgram1Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram2Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram3Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram4Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram5Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram6Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram7Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram8Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram9Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram10Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram11Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram12Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram13Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram14Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram15Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};

class BehavioralRV32ISystemProgram16Test : public BehavioralRV32ISystemProgramTestBase {
protected:
    RV32ISystemProgramCase getCase() const override;
};
