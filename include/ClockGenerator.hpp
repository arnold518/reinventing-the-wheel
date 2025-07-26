#pragma once
#include "BasicComponent.hpp"
#include <iostream>

class ClockGenerator : public BasicComponent
{
private:
    LogicValue current_state;
    size_t period_half;

public:
    ClockGenerator(std::string name, size_t half_period);
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
    void startClock(Simulator& simulator, size_t start_time);
};