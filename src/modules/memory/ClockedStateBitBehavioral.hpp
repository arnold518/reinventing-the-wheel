#pragma once

#include "components/BasicComponent.hpp"

class ClockedStateBitBehavioral : public BasicComponent {
public:
    explicit ClockedStateBitBehavioral(std::string name);
    const char* getTypeName() const override { return "ClockedStateBit"; }
    void evaluate(size_t current_time, Simulator& simulator) override;
private:
    LogicValue stored_ = LogicValue::UNKNOWN;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};
