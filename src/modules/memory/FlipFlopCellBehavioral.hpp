#pragma once

#include "simulator/SimulationTime.hpp"
// Private factory implementation; callers use families::FlipFlopCell.

#include "components/BasicComponent.hpp"

class FlipFlopCellBehavioral : public BasicComponent {
public:
    FlipFlopCellBehavioral(std::string name);
    static constexpr const char* TypeName = "FlipFlopCell";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
    StateSupport stateSupport() const override { return StateSupport::Saved; }
    void saveOwnState(circuit::state::Writer& out) const override;
    void restoreOwnState(circuit::state::Reader& in) override;

private:
    LogicValue stored_value;
    LogicValue previous_clk;
};
