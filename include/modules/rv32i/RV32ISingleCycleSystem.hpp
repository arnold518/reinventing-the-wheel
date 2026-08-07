#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RV32ISystemProgramAccess.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "rv32i/RV32IArchitecturalState.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace circuit::families {
extern const ComponentFamily RV32ISingleCycleSystem;
}

class RV32ISingleCycleSystem
    : public IOComponent,
      public RV32ISystemProgramAccess {
public:
    explicit RV32ISingleCycleSystem(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleSystem";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    std::shared_ptr<IOComponent> core() const { return core_; }
    std::shared_ptr<Memory64Kx32> instructionMemory() const { return instruction_memory_; }
    std::shared_ptr<Memory64Kx32> dataMemory() const { return data_memory_; }

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
    std::vector<uint8_t> readDataBytes(
        uint32_t base_address,
        size_t count) const override;
    void setProgramMemoryHistoryRecordingEnabled(
        bool enabled) override;
    void loadDataWords(uint32_t base_address, const std::vector<uint32_t>& words);

    rv32i::RV32IArchitecturalState
    snapshotArchitecturalState() const override;
    rv32i::RV32IMemoryTrace
    lastCommittedDataMemoryAccess() const override;
    std::map<uint32_t, uint8_t>
    dataMemoryWritesInTimeRange(
        size_t start_time,
        size_t end_time) const override;

private:
    std::shared_ptr<IOComponent> core_{};
    std::shared_ptr<Memory64Kx32> instruction_memory_{};
    std::shared_ptr<Memory64Kx32> data_memory_{};
};
