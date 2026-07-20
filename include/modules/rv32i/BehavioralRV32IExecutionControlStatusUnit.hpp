#pragma once

#include "components/BasicComponent.hpp"
#include <vector>

class BehavioralRV32IExecutionControlStatusUnit : public BasicComponent {
public:
    explicit BehavioralRV32IExecutionControlStatusUnit(std::string name);
    static constexpr const char* TypeName = "BehavioralRV32IExecutionControlStatusUnit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    LogicValue halted_ = LogicValue::UNKNOWN;
    LogicValue trapped_ = LogicValue::UNKNOWN;
    std::vector<LogicValue> trap_cause_{4, LogicValue::UNKNOWN};
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};
