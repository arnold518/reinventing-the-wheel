#pragma once
#include "components/BasicComponent.hpp"
#include <iostream>

class ClockGenerator : public BasicComponent
{
private:
    LogicValue current_state;
    size_t period_half;

public:
    ClockGenerator(std::string name, size_t half_period);
    static constexpr const char* TypeName = "ClockGenerator";
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
    void startClock(Simulator& simulator, size_t start_time);
};