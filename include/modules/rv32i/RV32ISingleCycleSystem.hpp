#pragma once

#include "components/IOComponent.hpp"
#include "modules/memory/BehavioralMemory64Kx32.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <memory>
#include <vector>

class RV32ISingleCycleSystem : public IOComponent {
public:
    explicit RV32ISingleCycleSystem(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleSystem";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    std::shared_ptr<RV32ISingleCycleCore> core() const { return core_; }
    std::shared_ptr<BehavioralMemory64Kx32> instructionMemory() const { return instruction_memory_; }
    std::shared_ptr<BehavioralMemory64Kx32> dataMemory() const { return data_memory_; }

    void clearInstructionMemory();
    void clearDataMemory();
    void loadProgram(const rv32i::RV32IProgram& program, uint32_t base_address = 0);
    void loadInstructionBytes(uint32_t base_address, const std::vector<uint8_t>& data);
    void loadDataBytes(uint32_t base_address, const std::vector<uint8_t>& data);
    void loadDataWords(uint32_t base_address, const std::vector<uint32_t>& words);

    rv32i::RV32IState snapshotState(uint64_t instruction_count = 0) const;

private:
    std::shared_ptr<RV32ISingleCycleCore> core_{};
    std::shared_ptr<BehavioralMemory64Kx32> instruction_memory_{};
    std::shared_ptr<BehavioralMemory64Kx32> data_memory_{};
};
