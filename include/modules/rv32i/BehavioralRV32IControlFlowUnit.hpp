#pragma once

#include "components/BasicComponent.hpp"
#include <vector>

class BehavioralRV32IControlFlowUnit : public BasicComponent {
public:
    explicit BehavioralRV32IControlFlowUnit(std::string name);
    static constexpr const char* TypeName = "BehavioralRV32IControlFlowUnit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    std::vector<LogicValue> pc_;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};
