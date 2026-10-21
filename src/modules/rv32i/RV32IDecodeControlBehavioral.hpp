#pragma once

#include "simulator/SimulationTime.hpp"
// Private factory implementation; callers use families::RV32IDecodeControl.

#include "components/BasicComponent.hpp"

class RV32IDecodeControlBehavioral : public BasicComponent {
public:
    explicit RV32IDecodeControlBehavioral(std::string name);
    static constexpr const char* TypeName = "RV32IDecodeControlUnit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
};
