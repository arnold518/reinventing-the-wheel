#pragma once

#include "components/IOComponent.hpp"
#include "modules/memory/BehavioralMemory64Kx32.hpp"
#include "modules/rv32i/BehavioralRV32ICore.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

class RV32ISystem : public IOComponent {
public:
    RV32ISystem(std::string name);
    static constexpr const char* TypeName = "RV32ISystem";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    std::shared_ptr<BehavioralRV32ICore> core() const { return core_; }
    std::shared_ptr<BehavioralMemory64Kx32> instructionMemory() const { return instruction_memory_; }
    std::shared_ptr<BehavioralMemory64Kx32> dataMemory() const { return data_memory_; }

    void setInitialPC(uint32_t pc);
    void setRegister(uint8_t index, uint32_t value);
    void resetCore();

    void clearInstructionMemory();
    void clearDataMemory();
    void loadProgram(const rv32i::RV32IProgram& program, uint32_t base_address = 0);
    void loadInstructionBytes(uint32_t base_address, const std::vector<uint8_t>& data);
    void loadDataBytes(uint32_t base_address, const std::vector<uint8_t>& data);
    void loadDataWords(uint32_t base_address, const std::vector<uint32_t>& words);

    rv32i::RV32IState snapshotState() const;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const;

private:
    std::shared_ptr<BehavioralRV32ICore> core_{};
    std::shared_ptr<BehavioralMemory64Kx32> instruction_memory_{};
    std::shared_ptr<BehavioralMemory64Kx32> data_memory_{};
};
