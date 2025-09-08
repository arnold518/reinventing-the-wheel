#pragma once

#include "components/BasicComponent.hpp"
#include <iostream>

class DFlipFlop : public BasicComponent
{
private:
    LogicValue current_q_state;
    LogicValue current_q_bar_state;
    LogicValue prev_clk_state;

public:
    DFlipFlop(std::string name);
    static constexpr const char* TypeName = "DFlipFlop";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};