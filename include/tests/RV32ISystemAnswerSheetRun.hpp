#pragma once

#include "basic/Wire.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include <cstddef>
#include <memory>

/**
 * One isolated execution of the evaluated RV32I answer sheet.
 *
 * This is a verification run used by RV32ISingleCycleSystemTest. It is not a
 * separately registered logical test and its implementation choice is never
 * encoded in a CTest or visualizer scenario name.
 */
class RV32IReferenceSystem;

class RV32ISystemAnswerSheetRun
    : public RV32IInstructionLockstepTest {
public:
    explicit RV32ISystemAnswerSheetRun(size_t program_number);

    void setupCircuit() override;
    size_t getRunDuration() const override;

protected:
    void buildCircuit() override;
    RV32ISystemProgramCase getCase() const override;
    void initializeComponentForLockstep(
        const RV32ISystemProgramCase& test_case) override;
    void clockComponentOneCycle(
        size_t cycle_index,
        size_t cycle_start_time) override;
    rv32i::RV32IState snapshotComponentState() const override;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const override;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const override;
    void verifyResults() override;

private:
    size_t program_number_;
    std::shared_ptr<RV32IReferenceSystem> system_{};
    std::shared_ptr<Wire<>> clk_wire_{};
    std::shared_ptr<Wire<>> rst_wire_{};
    std::shared_ptr<Wire<>> enable_wire_{};
    size_t visual_run_duration_ = 0;
    uint64_t committed_instruction_count_ = 0;
    mutable std::map<uint32_t, uint8_t> observed_bus_writes_{};
};
