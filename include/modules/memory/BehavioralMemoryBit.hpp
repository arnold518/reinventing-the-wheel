#pragma once

#include "components/BasicComponent.hpp"

class BehavioralMemoryBit : public BasicComponent {
public:
    BehavioralMemoryBit(std::string name);
    static constexpr const char* TypeName = "BehavioralMemoryBit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    LogicValue stored_value;
    LogicValue previous_clk;
};
