#pragma once

#include "rv32i/RV32IFunctionalMemory.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "rv32i/RV32IState.hpp"
#include "simulator/SimulationTest.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct RV32IMemoryInit {
    uint32_t address = 0;
    std::vector<uint8_t> bytes;
};

struct RV32IExpectedRegisterValue {
    uint8_t index = 0;
    uint32_t value = 0;
};

struct RV32ISystemExpectedResult {
    bool defined = false;
    uint32_t pc = 0;
    uint64_t instruction_count = 0;
    bool halted = false;
    bool trapped = false;
    rv32i::RV32IExecutionTrapCause trap_cause = rv32i::RV32IExecutionTrapCause::None;
    std::vector<RV32IExpectedRegisterValue> registers;
    std::map<uint32_t, uint8_t> bus_writes;
};

struct RV32ISystemProgramCase {
    std::string name;
    rv32i::RV32IProgram program;
    uint32_t program_base = 0;
    uint32_t initial_pc = 0;
    std::array<uint32_t, 32> initial_registers{};
    std::vector<RV32IMemoryInit> initial_data;
    size_t max_instructions = 128;
    size_t max_cycles_per_instruction = 16;
    size_t cycle_time_step = 10;
    size_t memory_size_bytes = rv32i::RV32IFunctionalMemory::DefaultCapacityBytes;
    RV32ISystemExpectedResult expected_result{};
};

class RV32IInstructionLockstepTest : public SimulationTest {
public:
    std::string getTestName() const override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    virtual RV32ISystemProgramCase getCase() const = 0;
    virtual void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) = 0;
    virtual void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) = 0;
    virtual rv32i::RV32IState snapshotComponentState() const = 0;
    virtual rv32i::RV32IMemoryTrace lastDataMemoryAccess() const = 0;
    virtual std::map<uint32_t, uint8_t> lastDataMemoryWrites() const = 0;

    void setInitialState() override;
    void runSimulation() override;
    void verifyResults() override;

private:
    RV32ISystemProgramCase test_case_{};
    rv32i::RV32IState oracle_state_{};
    rv32i::RV32IFunctionalMemory oracle_instruction_memory_{};
    rv32i::RV32IFunctionalMemory oracle_data_memory_{};
    uint64_t last_committed_instruction_count_ = 0;
    size_t total_cycles_ = 0;
    size_t time_origin_ = 0;
    bool completed_ = false;
    std::vector<SimulationCheckpoint> checkpoints_{};

    void setupOracle();
    void compareState(const rv32i::RV32IState& actual, const std::string& label) const;
    void compareMemoryEffects(const rv32i::RV32IMemoryTrace& expected, const std::string& label) const;
    [[noreturn]] void fail(const std::string& message) const;
};

class RV32IInstructionLockstepHarnessTest : public RV32IInstructionLockstepTest {
protected:
    void buildCircuit() override {}
    RV32ISystemProgramCase getCase() const override;
    void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) override;
    void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) override;
    rv32i::RV32IState snapshotComponentState() const override;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const override;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const override;

private:
    rv32i::RV32IState component_state_{};
    rv32i::RV32IFunctionalMemory component_instruction_memory_{};
    rv32i::RV32IFunctionalMemory component_data_memory_{};
    rv32i::RV32IMemoryTrace last_data_memory_access_{};
    std::map<uint32_t, uint8_t> last_data_memory_writes_{};
};

class RV32IInstructionLockstepMismatchDetectionTest : public SimulationTest {
protected:
    std::string getTestName() const override;
    void buildCircuit() override {}
    void setInitialState() override {}
    void verifyResults() override;
};
