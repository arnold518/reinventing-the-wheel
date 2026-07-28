#pragma once
// Private factory implementation; callers use families::RV32IControlFlow.

#include "components/BasicComponent.hpp"
#include <vector>

class RV32IControlFlowDirect : public BasicComponent {
public:
    explicit RV32IControlFlowDirect(std::string name);
    static constexpr const char* TypeName = "RV32IControlFlowUnit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    std::vector<LogicValue> pc_;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};
