#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"

class ClockedStateBitBehavioral : public BasicComponent {
public:
    explicit ClockedStateBitBehavioral(std::string name);
    const char* getTypeName() const override { return "ClockedStateBit"; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
    StateSupport stateSupport() const override { return StateSupport::Saved; }
    void saveOwnState(circuit::state::Writer& out) const override;
    void restoreOwnState(circuit::state::Reader& in) override;
private:
    LogicValue stored_ = LogicValue::UNKNOWN;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};
