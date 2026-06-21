#pragma once

#include "components/BasicComponent.hpp"
#include "modules/memory/BehavioralMemory64Kx32.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <map>
#include <memory>

class BehavioralRV32ICore : public BasicComponent {
public:
    BehavioralRV32ICore(std::string name);
    static constexpr const char* TypeName = "BehavioralRV32ICore";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

    void attachMemories(std::shared_ptr<BehavioralMemory64Kx32> instruction_memory,
                        std::shared_ptr<BehavioralMemory64Kx32> data_memory);

    void setInitialPC(uint32_t pc);
    void setRegister(uint8_t index, uint32_t value);
    void resetCore();

    rv32i::RV32IState snapshotState() const;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const;

private:
    uint32_t reset_pc_ = 0;
    rv32i::RV32IState state_{};
    std::weak_ptr<BehavioralMemory64Kx32> instruction_memory_;
    std::weak_ptr<BehavioralMemory64Kx32> data_memory_;
    rv32i::RV32IMemoryTrace last_data_memory_access_{};
    std::map<uint32_t, uint8_t> last_data_memory_writes_{};
    uint32_t last_instruction_address_ = 0;
    bool last_instruction_read_ = false;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;

    void publishOutputs(Simulator& simulator, size_t current_time, bool active_core_step);
};
