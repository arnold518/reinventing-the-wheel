#pragma once
// Private factory implementation; callers use families::RV32ISingleCycleCore.

#include "components/BasicComponent.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "rv32i/RV32IArchitecturalState.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <map>
#include <memory>

class RV32IReferenceCore : public BasicComponent, public RV32IStateView {
public:
    RV32IReferenceCore(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleCore";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

    void setInitialPC(uint32_t pc);
    void setRegister(uint8_t index, uint32_t value);
    void resetCore();

    rv32i::RV32IArchitecturalState
    snapshotArchitecturalState() const override;
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const;
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const;

private:
    uint32_t reset_pc_ = 0;
    rv32i::RV32IState state_{};
    rv32i::RV32IMemoryTrace last_data_memory_access_{};
    std::map<uint32_t, uint8_t> last_data_memory_writes_{};
    LogicValue previous_clk_ = LogicValue::UNKNOWN;

    void publishOutputs(Simulator& simulator, size_t current_time);
};
