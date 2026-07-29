#pragma once
// Private factory implementation; callers use families::RV32ISingleCycleSystem.

#include "components/BasicComponent.hpp"
#include "components/capabilities/RV32ISystemProgramAccess.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "rv32i/RV32IArchitecturalState.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

/**
 * Evaluated answer-sheet implementation of the RV32I single-cycle system
 * contract. The catalog exposes it as the behavioral fidelity of
 * RV32ISingleCycleSystem; its historical class name is not a public contract.
 */
class RV32IReferenceSystem
    : public BasicComponent,
      public RV32ISystemProgramAccess {
public:
    explicit RV32IReferenceSystem(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleSystem";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

    std::shared_ptr<Memory64Kx32> instructionMemory() const {
        return instruction_memory_;
    }
    std::shared_ptr<Memory64Kx32> dataMemory() const {
        return data_memory_;
    }

    void setInitialPC(uint32_t pc);
    void setRegister(uint8_t index, uint32_t value);
    void resetCore();

    void clearInstructionMemory() override;
    void clearDataMemory() override;
    void loadProgram(
        const rv32i::RV32IProgram& program,
        uint32_t base_address = 0) override;
    void loadInstructionBytes(
        uint32_t base_address,
        const std::vector<uint8_t>& data) override;
    void loadDataBytes(
        uint32_t base_address,
        const std::vector<uint8_t>& data) override;
    void loadDataWords(
        uint32_t base_address,
        const std::vector<uint32_t>& words);

    rv32i::RV32IArchitecturalState snapshotArchitecturalState() const override {
        return rv32i::RV32IArchitecturalState::fromKnown(state_);
    }
    rv32i::RV32IMemoryTrace
    lastCommittedDataMemoryAccess() const override {
        return last_data_memory_access_;
    }
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const {
        return lastCommittedDataMemoryAccess();
    }
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const {
        return last_data_memory_writes_;
    }
    std::map<uint32_t, uint8_t>
    dataMemoryWritesInTimeRange(
        size_t start_time,
        size_t end_time) const override;

private:
    struct DataMemoryWrite {
        size_t time = 0;
        uint32_t address = 0;
        uint8_t value = 0;
    };

    uint32_t reset_pc_ = 0;
    rv32i::RV32IState state_{};
    std::shared_ptr<Memory64Kx32> instruction_memory_;
    std::shared_ptr<Memory64Kx32> data_memory_;
    rv32i::RV32IMemoryTrace last_data_memory_access_{};
    std::map<uint32_t, uint8_t> last_data_memory_writes_{};
    std::vector<DataMemoryWrite> data_memory_write_history_{};
    LogicValue previous_clk_ = LogicValue::UNKNOWN;

    void applyDataMemoryWrite(size_t current_time);
    void publishOutputs(Simulator& simulator, size_t current_time);
};
