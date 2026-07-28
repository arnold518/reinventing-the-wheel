#pragma once
// Private factory implementation; callers use families::RV32IExecutionStatus.

#include "components/BasicComponent.hpp"
#include <vector>

class RV32IExecutionStatusDirect : public BasicComponent {
public:
    explicit RV32IExecutionStatusDirect(std::string name);
    static constexpr const char* TypeName = "RV32IExecutionControlStatusUnit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    LogicValue halted_ = LogicValue::UNKNOWN;
    LogicValue trapped_ = LogicValue::UNKNOWN;
    std::vector<LogicValue> trap_cause_{4, LogicValue::UNKNOWN};
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};
