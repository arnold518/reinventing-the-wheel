#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"
#include <iostream>

class ClockGenerator : public BasicComponent
{
private:
    LogicValue current_state;
    SimulationTime period_half;

public:
    ClockGenerator(std::string name, SimulationTime half_period);
    static constexpr const char* TypeName = "ClockGenerator";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
    void startClock(Simulator& simulator, SimulationTime start_time);
};
